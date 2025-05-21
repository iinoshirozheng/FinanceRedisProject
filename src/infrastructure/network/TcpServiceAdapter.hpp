#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <vector>  // Required for consumer's tmp buffer
#include <cstring> // For strerror, memset
#include <cerrno>  // For errno

// Linux Socket Headers
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> // For close()
#include <fcntl.h>  // For fcntl() if using non-blocking sockets

#include "RingBuffer.hpp"
#include "infrastructure/config/ConnectionConfigProvider.hpp"
#include "domain/IPackageHandler.hpp"
#include "domain/IFinanceRepository.hpp"
#include "domain/FinanceDataStructure.hpp"
#include "utils/FinanceUtils.hpp" // Not directly used in this file but kept for context
#include <loguru.hpp>

namespace finance::infrastructure::network
{
    static constexpr size_t RING_BUFFER_SIZE = 16 * 1024 * 1024; // 16MB
    // Backlog for listen()
    static constexpr int SOCKET_LISTEN_BACKLOG = SOMAXCONN; // Or a specific number like 128

    class TcpServiceAdapter
    {
    public:
        explicit TcpServiceAdapter(std::shared_ptr<finance::domain::IPackageHandler> handler,
                                   std::shared_ptr<finance::domain::IFinanceRepository<finance::domain::SummaryData, finance::domain::ErrorResult>> repository)
            : serverSocketFd_(-1), // Initialize server socket descriptor to an invalid value
              handler_(std::move(handler)),
              repository_(std::move(repository)),
              ringBuffer_()
        {
            serverSocketFd_ = socket(AF_INET, SOCK_STREAM, 0);
            if (serverSocketFd_ < 0)
            {
                LOG_F(FATAL, "Failed to create server socket: %s", strerror(errno));
                throw std::runtime_error("Failed to create server socket: " + std::string(strerror(errno)));
            }

            int opt = 1;
            if (setsockopt(serverSocketFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
            {
                LOG_F(WARNING, "Failed to set SO_REUSEADDR: %s", strerror(errno));
                // Not fatal, but log it
            }
            // SO_REUSEPORT might not be available/default on all systems, use with caution or ifdef
            if (setsockopt(serverSocketFd_, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0)
            {
                LOG_F(WARNING, "Failed to set SO_REUSEPORT: %s", strerror(errno));
                // Not fatal
            }

            sockaddr_in serverAddr;
            memset(&serverAddr, 0, sizeof(serverAddr));
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_addr.s_addr = INADDR_ANY; // Listen on 0.0.0.0
            serverAddr.sin_port = htons(static_cast<unsigned short>(config::ConnectionConfigProvider::serverPort()));

            if (bind(serverSocketFd_, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0)
            {
                LOG_F(FATAL, "Failed to bind server socket to port %d: %s", config::ConnectionConfigProvider::serverPort(), strerror(errno));
                close(serverSocketFd_);
                throw std::runtime_error("Failed to bind server socket: " + std::string(strerror(errno)));
            }

            if (listen(serverSocketFd_, SOCKET_LISTEN_BACKLOG) < 0)
            {
                LOG_F(FATAL, "Failed to listen on server socket: %s", strerror(errno));
                close(serverSocketFd_);
                throw std::runtime_error("Failed to listen on server socket: " + std::string(strerror(errno)));
            }
            LOG_F(INFO, "Server socket listening on port %d", config::ConnectionConfigProvider::serverPort());
        }

        ~TcpServiceAdapter()
        {
            stop();
        }

        bool start()
        {
            if (running_.load())
                return false;

            if (serverSocketFd_ < 0)
            {
                LOG_F(ERROR, "Cannot start, server socket is not initialized.");
                return false;
            }

            running_ = true;
            acceptThread_ = std::thread(&TcpServiceAdapter::producer, this);
            processingThread_ = std::thread(&TcpServiceAdapter::consumer, this);
            return true;
        }

        // In TcpServiceAdapter::stop()
        void stop()
        {
            bool expected_running = true;
            // Attempt to change running_ from true to false.
            // If it was already false, another thread is stopping/has stopped it.
            if (!running_.compare_exchange_strong(expected_running, false))
            {
                LOG_F(INFO, "TcpServiceAdapter::stop() called but already stopping or stopped.");
                // If threads might still be joinable from a previous incomplete stop,
                // you might still want to attempt to join them, but be cautious.
                // For now, just return if already stopping.
                if (acceptThread_.joinable() && acceptThread_.get_id() != std::this_thread::get_id())
                    acceptThread_.join();
                if (processingThread_.joinable() && processingThread_.get_id() != std::this_thread::get_id())
                    processingThread_.join();
                return;
            }

            LOG_F(INFO, "TcpServiceAdapter: Initiating stop sequence...");

            if (serverSocketFd_ >= 0)
            {
                LOG_F(INFO, "TcpServiceAdapter: Shutting down and closing server socket fd: %d", serverSocketFd_);
                // Shutdown first to break any blocking calls on the socket.
                // SHUT_RD will prevent further reads on the socket, helping accept() to unblock.
                if (shutdown(serverSocketFd_, SHUT_RD) < 0)
                { // SHUT_RD is often enough to unblock accept
                    LOG_F(WARNING, "TcpServiceAdapter: shutdown(serverSocketFd_, SHUT_RD) failed: %s", strerror(errno));
                }
                if (close(serverSocketFd_) < 0)
                {
                    LOG_F(WARNING, "TcpServiceAdapter: close(serverSocketFd_) failed: %s", strerror(errno));
                }
                serverSocketFd_ = -1;
            }

            // producer thread should exit as running_ is false and accept() unblocks.
            if (acceptThread_.joinable())
            {
                LOG_F(INFO, "TcpServiceAdapter: Joining accept thread...");
                acceptThread_.join();
                LOG_F(INFO, "TcpServiceAdapter: Accept thread joined.");
            }

            // consumer thread should exit as running_ is false.
            // RingBuffer::waitForData() is a spin lock but consumer's outer loop checks running_.
            if (processingThread_.joinable())
            {
                LOG_F(INFO, "TcpServiceAdapter: Joining processing thread...");
                processingThread_.join();
                LOG_F(INFO, "TcpServiceAdapter: Processing thread joined.");
            }
            LOG_F(INFO, "TcpServiceAdapter: Stop sequence completed.");
        }

    private:
        std::string getPeerAddress(const sockaddr_in &addr)
        {
            char ipStr[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &addr.sin_addr, ipStr, sizeof(ipStr));
            return std::string(ipStr) + ":" + std::to_string(ntohs(addr.sin_port));
        }

        // TcpServiceAdapter.hpp - producer() method
        void producer()
        {
            LOG_F(INFO, "Producer thread started. TID: %zu. running_ initial value: %d",
                  std::hash<std::thread::id>{}(std::this_thread::get_id()),
                  running_.load(std::memory_order_relaxed));
            try
            {
                while (running_.load(std::memory_order_relaxed))
                {
                    sockaddr_in clientAddr;
                    socklen_t clientAddrLen = sizeof(clientAddr);
                    int clientSocketFd = -1;

                    LOG_F(INFO, "Producer (TID: %zu): Loop top. Attempty to accept. running_=%d, serverSocketFd_=%d",
                          std::hash<std::thread::id>{}(std::this_thread::get_id()),
                          running_.load(std::memory_order_relaxed), serverSocketFd_);

                    if (serverSocketFd_ < 0)
                    { // 如果 socket 已經無效，直接退出
                        LOG_F(WARNING, "Producer (TID: %zu): serverSocketFd_ is invalid (%d) before accept. Exiting.",
                              std::hash<std::thread::id>{}(std::this_thread::get_id()), serverSocketFd_);
                        break;
                    }

                    clientSocketFd = accept(serverSocketFd_, (struct sockaddr *)&clientAddr, &clientAddrLen);
                    int accept_errno = errno; // Store errno immediately after accept returns

                    LOG_F(INFO, "Producer (TID: %zu): accept returned %d. errno: %d (%s). running_=%d",
                          std::hash<std::thread::id>{}(std::this_thread::get_id()),
                          clientSocketFd, accept_errno, strerror(accept_errno),
                          running_.load(std::memory_order_relaxed));

                    if (!running_.load(std::memory_order_relaxed))
                    {
                        if (clientSocketFd >= 0)
                        {
                            LOG_F(INFO, "Producer (TID: %zu): Closing client socket %d as running_ is false.",
                                  std::hash<std::thread::id>{}(std::this_thread::get_id()), clientSocketFd);
                            close(clientSocketFd);
                        }
                        LOG_F(INFO, "Producer (TID: %zu): running_ is false after accept unblocked. Exiting main loop.",
                              std::hash<std::thread::id>{}(std::this_thread::get_id()));
                        break;
                    }

                    if (clientSocketFd < 0)
                    {
                        // EBADF or EINVAL are expected if the listening socket was closed by stop()
                        if (accept_errno == EBADF || accept_errno == EINVAL)
                        {
                            LOG_F(INFO, "Producer (TID: %zu): accept() failed due to server socket closure (errno %d: %s). Exiting main loop.",
                                  std::hash<std::thread::id>{}(std::this_thread::get_id()),
                                  accept_errno, strerror(accept_errno));
                            break;
                        }
                        if (accept_errno == EINTR)
                        {
                            LOG_F(INFO, "Producer (TID: %zu): accept() call interrupted by signal. Continuing loop to re-check running_ flag.",
                                  std::hash<std::thread::id>{}(std::this_thread::get_id()));
                            continue;
                        }
                        // For other errors, log and continue (or break if appropriate)
                        LOG_F(ERROR, "Producer (TID: %zu): Accept connection error (errno %d: %s). Continuing.",
                              std::hash<std::thread::id>{}(std::this_thread::get_id()),
                              accept_errno, strerror(accept_errno));
                        std::this_thread::sleep_for(std::chrono::milliseconds(50)); // Avoid tight loop on persistent accept errors
                        continue;
                    }

                    // ... (後續客戶端處理邏輯不變, 但也應包含TID日誌和running_檢查) ...
                    LOG_F(INFO, "Producer (TID: %zu): Accepted new connection from %s on fd %d",
                          std::hash<std::thread::id>{}(std::this_thread::get_id()),
                          getPeerAddress(clientAddr).c_str(), clientSocketFd);

                    // TcpServiceAdapter.hpp - producer() method - Inner client handling loop excerpt
                    // ... (accepts clientSocketFd) ...
                    LOG_F(INFO, "Producer (TID: %zu): Accepted new connection from %s on fd %d",
                          std::hash<std::thread::id>{}(std::this_thread::get_id()),
                          getPeerAddress(clientAddr).c_str(), clientSocketFd);

                    // Inner client handling loop
                    while (running_.load(std::memory_order_relaxed))
                    {
                        size_t maxLen;
                        char *writePtr = ringBuffer_.writablePtr(maxLen); // 獲取 RingBuffer 的可寫入指標和長度

                        if (maxLen == 0)
                        { // 如果 RingBuffer 滿了
                            if (!running_.load(std::memory_order_relaxed))
                                break;
                            LOG_F(INFO, "Producer (TID: %zu, client fd %d): RingBuffer full or no space. Yielding.",
                                  std::hash<std::thread::id>{}(std::this_thread::get_id()), clientSocketFd);
                            std::this_thread::yield();
                            continue;
                        }

                        // Ensure to log TID here as well and check running_ frequently
                        if (!running_.load(std::memory_order_relaxed))
                        {
                            LOG_F(INFO, "Producer (TID: %zu, client fd %d): running_ is false in client loop. Breaking.",
                                  std::hash<std::thread::id>{}(std::this_thread::get_id()), clientSocketFd);
                            break;
                        }

                        LOG_F(INFO, "Producer (TID: %zu, client fd %d): Attempting to recv. maxLen=%zu",
                              std::hash<std::thread::id>{}(std::this_thread::get_id()), clientSocketFd, maxLen);
                        ssize_t n = recv(clientSocketFd, writePtr, maxLen, 0); // <<< 여기가 recv 로직입니다.
                        int recv_errno = errno;                                // Store errno immediately

                        LOG_F(INFO, "Producer (TID: %zu, client fd %d): recv returned %zd. errno: %d (%s). running_=%d",
                              std::hash<std::thread::id>{}(std::this_thread::get_id()),
                              clientSocketFd, n, recv_errno, strerror(recv_errno),
                              running_.load(std::memory_order_relaxed));

                        if (!running_.load(std::memory_order_relaxed))
                        { // Check running_ immediately after recv returns
                            LOG_F(INFO, "Producer (TID: %zu, client fd %d): running_ is false after recv. Exiting client loop.",
                                  std::hash<std::thread::id>{}(std::this_thread::get_id()), clientSocketFd);
                            break;
                        }

                        if (n > 0)
                        { // 成功接收到數據
                            ringBuffer_.enqueue(static_cast<size_t>(n));
                            LOG_F(INFO, "Producer (client fd %d): Enqueued %zd bytes.", clientSocketFd, n);
                        }
                        else if (n == 0)
                        { // 客戶端正常關閉連接
                            LOG_F(INFO, "Producer (client fd %d): Client disconnected normally.", clientSocketFd);
                            break; // 退出客戶端處理循環
                        }
                        else
                        { // n < 0, recv 發生錯誤
                            if (recv_errno == EINTR)
                            { // 被信號中斷
                                LOG_F(INFO, "Producer (client fd %d): recv() interrupted. Continuing client loop.", clientSocketFd);
                                continue; // 繼續嘗試接收
                            }
                            if (recv_errno == EAGAIN || recv_errno == EWOULDBLOCK)
                            { // 非阻塞模式下無數據，或超時 (如果設置了超時)
                                LOG_F(INFO, "Producer (client fd %d): recv() timeout or wouldblock.", clientSocketFd);
                                // 如果是阻塞socket且沒有超時，理論上不應頻繁出現此錯誤
                                // 短暫讓出CPU，然後繼續嘗試
                                if (!running_.load(std::memory_order_relaxed))
                                    break;
                                std::this_thread::yield();
                                continue;
                            }
                            // 其他錯誤
                            LOG_F(ERROR, "Producer (client fd %d): Receive error (errno %d: %s)", clientSocketFd, recv_errno, strerror(recv_errno));
                            break; // 發生錯誤，退出客戶端處理循環
                        }
                    } // End inner client handling loop

                    LOG_F(INFO, "Producer (TID: %zu): Closing client socket fd %d.",
                          std::hash<std::thread::id>{}(std::this_thread::get_id()), clientSocketFd);
                    close(clientSocketFd); // 關閉客戶端 socket

                    if (!running_.load(std::memory_order_relaxed))
                    {
                        LOG_F(INFO, "Producer (TID: %zu): running_ is false after client handling. Exiting main loop.",
                              std::hash<std::thread::id>{}(std::this_thread::get_id()));
                        break;
                    }
                } // End main accept loop
            }
            catch (const std::exception &e)
            {
                LOG_F(ERROR, "Producer thread exception: %s", e.what());
            }
            catch (...)
            {
                LOG_F(ERROR, "Producer thread unknown exception");
            }
            LOG_F(INFO, "Producer thread stopped. TID: %zu",
                  std::hash<std::thread::id>{}(std::this_thread::get_id()));
        }

        // TcpServiceAdapter.hpp - consumer() method
        void consumer()
        {
            LOG_F(INFO, "Consumer thread started (Synchronous processing).");
            std::vector<char> tmp;
            tmp.reserve(sizeof(domain::FinancePackageMessage) + 100);

            while (running_.load()) // 主要退出條件
            {
                // 等待數據或running_變為false
                // 可以給waitForData增加一個超時或使其可中斷，但更簡單的是在外部檢查
                if (ringBuffer_.empty())
                { // 如果為空
                    if (!running_.load())
                    {          // 再次檢查 running_
                        break; // 如果準備停止，則退出
                    }
                    std::this_thread::yield(); // 短暫讓出CPU，避免純粹的忙等待空 RingBuffer
                    // 或者使用一個帶有超時的條件變數來等待數據，但這會改變 RingBuffer 的設計理念
                    continue; // 重新開始循環，檢查 running_ 和是否有數據
                }

                // 到這裡時，RingBuffer 不為空 (或 running_ 在上面已經使其跳出)
                // 即使 ringBuffer_.waitForData(); 被調用，也要確保它不會無限阻塞
                // 但由於 waitForData 是自旋，我們主要靠 running_.load() 來退出
                // ringBuffer_.waitForData(); // 如果保留，確保它不會永遠自旋

                if (!running_.load()) // 在嘗試獲取封包前再次檢查
                    break;

                auto segOpt = ringBuffer_.getNextPacket();
                if (!segOpt)
                {
                    if (!running_.load())
                    { // 如果沒有封包且準備停止
                        break;
                    }
                    std::this_thread::yield(); // 如果沒有完整封包，讓出 CPU
                    continue;
                }

                // ... (後續處理不變)
                auto seg = *segOpt;

                if (seg.totalLen() <= 3)
                {
                    LOG_F(INFO, "Consumer: Dropping potential keep alive packet with size %zu", seg.totalLen());
                    ringBuffer_.dequeue(seg.totalLen());
                    continue;
                }

                const domain::FinancePackageMessage *pkg = nullptr;
                if (seg.len2 == 0)
                {
                    pkg = reinterpret_cast<const domain::FinancePackageMessage *>(seg.ptr1);
                }
                else
                {
                    tmp.clear();
                    if (tmp.capacity() < seg.totalLen())
                    {
                        tmp.reserve(seg.totalLen() + 100);
                    }
                    tmp.resize(seg.totalLen());
                    std::memcpy(tmp.data(), seg.ptr1, seg.len1);
                    std::memcpy(tmp.data() + seg.len1, seg.ptr2, seg.len2);
                    pkg = reinterpret_cast<const domain::FinancePackageMessage *>(tmp.data());
                }

                auto res = handler_->handle(*pkg);

                if (res.is_err())
                {
                    LOG_F(ERROR, "Consumer: Packet handling failed for packet size %zu: %s", seg.totalLen(), res.unwrap_err().message.c_str());
                }

                ringBuffer_.dequeue(seg.totalLen());
            }

            LOG_F(INFO, "Consumer thread stopped.");
        }

        int serverSocketFd_; // Server's listening socket descriptor
        std::shared_ptr<finance::domain::IPackageHandler> handler_;
        std::shared_ptr<finance::domain::IFinanceRepository<finance::domain::SummaryData, finance::domain::ErrorResult>> repository_;
        RingBuffer<RING_BUFFER_SIZE> ringBuffer_;
        std::thread acceptThread_;
        std::thread processingThread_;
        std::atomic<bool> running_{false};
        // cvMutex_ and cv_ are not directly used by socket operations anymore for unblocking,
        // but might be kept if other logic relies on them.
        // For now, their direct utility for socket thread synchronization is reduced.
        // std::mutex cvMutex_;
        // std::condition_variable cv_;
    };
} // namespace finance::infrastructure::network