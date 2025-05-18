#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/StreamSocket.h>
#include "RingBuffer.hpp"
#include "infrastructure/config/ConnectionConfigProvider.hpp"
#include "domain/IPackageHandler.hpp"
#include "domain/IFinanceRepository.hpp"
#include "domain/FinanceDataStructure.hpp"
#include "utils/FinanceUtils.hpp"
#include <loguru.hpp>

namespace finance::infrastructure::network
{
    static constexpr size_t RING_BUFFER_SIZE = 16 * 1024 * 1024; // 16MB

    class TcpServiceAdapter
    {
    public:
        explicit TcpServiceAdapter(std::shared_ptr<finance::domain::IPackageHandler> handler,
                                   std::shared_ptr<finance::domain::IFinanceRepository<finance::domain::SummaryData, finance::domain::ErrorResult>> repository)
            : serverSocket_{{"0.0.0.0", static_cast<unsigned short>(config::ConnectionConfigProvider::serverPort())}},
              handler_(std::move(handler)),
              repository_(std::move(repository)),
              ringBuffer_()
        {
            serverSocket_.setReuseAddress(true);
            serverSocket_.setReusePort(true);
            // serverSocket_.setReceiveTimeout(Poco::Timespan(config::ConnectionConfigProvider::socketTimeoutMs() * 1000));
        }

        ~TcpServiceAdapter()
        {
            stop();
        }

        bool start()
        {
            if (running_)
                return false;

            running_ = true;
            acceptThread_ = std::thread(&TcpServiceAdapter::producer, this);
            processingThread_ = std::thread(&TcpServiceAdapter::consumer, this);
            return true;
        }

        void stop()
        {
            running_ = false;
            serverSocket_.close();
            cv_.notify_all();

            if (acceptThread_.joinable())
                acceptThread_.join();
            if (processingThread_.joinable())
                processingThread_.join();
        }

    private:
        // producer() 方法保持不變，它負責接收數據並放入 RingBuffer
        void producer()
        {
            LOG_F(INFO, "Producer thread started.");
            try
            {
                while (running_)
                {
                    Poco::Net::StreamSocket clientSocket;
                    try
                    {
                        clientSocket = serverSocket_.acceptConnection();
                        LOG_F(INFO, "Accepted new connection from %s", clientSocket.peerAddress().toString().c_str());
                    }
                    catch (const Poco::Exception &exc)
                    {
                        if (running_)
                        {
                            LOG_F(ERROR, "Producer: Accept connection error: %s", exc.displayText().c_str());
                        }
                        else
                        {
                            LOG_F(INFO, "Producer thread shutting down due to Poco exception during stop.");
                        }
                        continue;
                    }

                    if (!clientSocket.impl()->initialized())
                    {
                        LOG_F(ERROR, "Producer: clientSocket initialized failed !");
                        continue;
                    }
                    // clientSocket.setReceiveTimeout( Poco::Timespan(config::ConnectionConfigProvider::socketTimeoutMs() * 1000));

                    while (running_)
                    {
                        size_t maxLen;
                        char *writePtr = ringBuffer_.writablePtr(maxLen);

                        if (maxLen == 0)
                        {
                            std::this_thread::yield();
                            continue;
                        }

                        size_t n = 0;
                        try
                        {
                            n = clientSocket.receiveBytes(writePtr, maxLen);
                        }
                        catch (const Poco::TimeoutException &)
                        {
                            // LOG_F(DEBUG, "Producer: Receive timeout from %s", clientSocket.peerAddress().toString().c_str());
                            continue;
                        }
                        catch (const Poco::Exception &exc)
                        {
                            LOG_F(ERROR, "Producer: Receive error from %s: %s", clientSocket.peerAddress().toString().c_str(), exc.displayText().c_str());
                            break;
                        }

                        if (n > 0)
                        {
                            ringBuffer_.enqueue(n);
                            LOG_F(INFO, "Producer: Enqueued %zu bytes.", n);
                        }
                        else if (n == 0)
                        {
                            LOG_F(INFO, "Producer: Client %s disconnected normally.", clientSocket.peerAddress().toString().c_str());
                            break;
                        }
                    } // end while(running_) for single client

                    clientSocket.close();
                    LOG_F(INFO, "Producer: Client socket closed.");

                } // end while(running_) for accepting connections
            }
            catch (const Poco::Exception &exc)
            {
                if (running_)
                {
                    LOG_F(ERROR, "Producer thread Poco exception: %s", exc.displayText().c_str());
                }
                else
                {
                    LOG_F(INFO, "Producer thread shutting down due to Poco exception during stop.");
                }
            }
            catch (const std::exception &e)
            {
                LOG_F(ERROR, "Producer thread standard exception: %s", e.what());
            }
            catch (...)
            {
                LOG_F(ERROR, "Unknown exception in producer thread");
            }
            LOG_F(INFO, "Producer thread stopped.");
        }

        // consumer() 方法現在同步處理封包和後續操作
        void consumer()
        {
            LOG_F(INFO, "Consumer thread started (Synchronous processing).");
            std::vector<char> tmp;
            tmp.reserve(sizeof(domain::FinancePackageMessage) + 100);

            while (running_)
            {
                // 使用 RingBuffer 的原生等待機制，直到有資料可讀或執行緒停止
                // RingBuffer::waitForData() 會自旋等待
                ringBuffer_.waitForData();

                if (!running_)
                    break;

                auto segOpt = ringBuffer_.getNextPacket();
                if (!segOpt)
                {
                    continue;
                }
                auto seg = *segOpt;

                // 心跳過濾
                if (seg.totalLen() < sizeof(domain::FinancePackageMessage) - sizeof(domain::ApData) - 10)
                {
                    LOG_F(INFO, "Consumer: Dropping potential keep alive packet with size %zu", seg.totalLen());
                    ringBuffer_.dequeue(seg.totalLen());
                    continue;
                }

                // 重組跨界封包
                const domain::FinancePackageMessage *pkg = nullptr;
                if (seg.len2 == 0)
                {
                    pkg = reinterpret_cast<const domain::FinancePackageMessage *>(seg.ptr1);
                }
                else
                {
                    tmp.clear();
                    tmp.resize(seg.totalLen());
                    std::memcpy(tmp.data(), seg.ptr1, seg.len1);
                    std::memcpy(tmp.data() + seg.len1, seg.ptr2, seg.len2);
                    pkg = reinterpret_cast<const domain::FinancePackageMessage *>(tmp.data());
                    // LOG_F(DEBUG, "Consumer: Reassembled packet size %zu from two segments.", seg.totalLen());
                }

                // *** 同步處理封包 ***
                // handler_->handle 方法現在不接受 TaskQueue 參數
                // handle 方法內部會直接呼叫 repo_->sync() 和 repo_->update()
                auto res = handler_->handle(*pkg); // 這一步可能會阻塞

                if (res.is_err())
                {
                    LOG_F(ERROR, "Consumer: Packet handling failed for packet size %zu: %s", seg.totalLen(), res.unwrap_err().message.c_str());
                    // 錯誤處理
                }

                // 從 RingBuffer 中移除已處理的封包數據
                size_t len = seg.totalLen();
                // LOG_F(INFO, "Consumer: Dequeueing packet of size %zu", len);
                ringBuffer_.dequeue(len);
            } // end while(running_) for consumer loop

            LOG_F(INFO, "Consumer thread stopped.");
        }

        Poco::Net::ServerSocket serverSocket_;
        std::shared_ptr<finance::domain::IPackageHandler> handler_;
        std::shared_ptr<finance::domain::IFinanceRepository<finance::domain::SummaryData, finance::domain::ErrorResult>> repository_;
        RingBuffer<RING_BUFFER_SIZE> ringBuffer_;
        std::thread acceptThread_;
        std::thread processingThread_;
        std::atomic<bool> running_{false};
        std::mutex cvMutex_;
        std::condition_variable cv_;
    };
}