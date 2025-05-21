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
#include "infrastructure/tasks/RedisWorker.hpp"
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
              redis_worker_(std::make_unique<tasks::RedisWorker>(repository_)),
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
            redis_worker_->start();
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

            redis_worker_->stop();

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

        void consumer()
        {
            LOG_F(INFO, "Consumer thread started (Asynchronous Redis processing).");
            std::vector<char> tmp;
            tmp.reserve(sizeof(domain::FinancePackageMessage) + 100);

            while (running_.load())
            {
                if (ringBuffer_.empty())
                {
                    if (!running_.load())
                    {
                        break;
                    }
                    std::this_thread::yield();
                    continue;
                }

                if (!running_.load())
                    break;

                auto segOpt = ringBuffer_.getNextPacket();
                if (!segOpt)
                {
                    if (!running_.load())
                    {
                        break;
                    }
                    std::this_thread::yield();
                    continue;
                }

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
                    LOG_F(ERROR, "Consumer: Packet handling/task submission failed for packet size %zu: %s",
                          seg.totalLen(), res.unwrap_err().message.c_str());
                }
                else
                {
                    LOG_F(INFO, "Consumer: Packet processed and async Redis tasks submitted for packet size %zu.",
                          seg.totalLen());
                }

                ringBuffer_.dequeue(seg.totalLen());
            }

            LOG_F(INFO, "Consumer thread stopped.");
        }

        void wait()
        {
            if (acceptThread_.joinable())
            {
                acceptThread_.join();
            }
            if (processingThread_.joinable())
            {
                processingThread_.join();
            }
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

        int serverSocketFd_; // Server's listening socket descriptor
        std::shared_ptr<finance::domain::IPackageHandler> handler_;
        std::shared_ptr<finance::domain::IFinanceRepository<finance::domain::SummaryData, finance::domain::ErrorResult>> repository_;
        std::unique_ptr<tasks::RedisWorker> redis_worker_;
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