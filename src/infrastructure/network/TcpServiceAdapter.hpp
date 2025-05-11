#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Exception.h>
#include "RingBuffer.hpp"
#include "../config/ConnectionConfigProvider.hpp"
#include "../../domain/IPackageHandler.h"
#include "../../domain/IFinanceRepository.h"
#include "../../domain/FinanceDataStructure.h"
#include "../../utils/FinanceUtils.hpp"
#include <loguru.hpp>

namespace finance::infrastructure::network
{
    using config::ConnectionConfigProvider;
    using domain::ErrorResult;
    using domain::Result;
    using utils::FinanceUtils;

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
            serverSocket_.setReceiveTimeout(Poco::Timespan(config::ConnectionConfigProvider::socketTimeoutMs() * 1000));
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
            if (!running_)
                return;

            running_ = false;
            serverSocket_.close();
            cv_.notify_all();

            if (acceptThread_.joinable())
                acceptThread_.join();
            if (processingThread_.joinable())
                processingThread_.join();
        }

    private:
        void producer()
        {
            try
            {
                Poco::Net::StreamSocket clientSocket;

                while (running_)
                {
                    try
                    {
                        clientSocket = serverSocket_.acceptConnection();
                        clientSocket.setReceiveTimeout(
                            Poco::Timespan(
                                ConnectionConfigProvider::socketTimeoutMs() * 1000));

                        while (running_)
                        {
                            // 1) 先拿到可連續寫入的位置與長度
                            size_t maxLen;
                            char *writePtr = ringBuffer_.writablePtr(maxLen);
                            if (maxLen == 0)
                            {
                                // 緩衝區滿，輕自旋或稍微 sleep
                                std::this_thread::yield();
                                continue;
                            }

                            // 2) 直接從 socket 讀到這塊記憶體
                            int n = clientSocket.receiveBytes(writePtr, maxLen);
                            if (n <= 0)
                                break;

                            // 3) 發佈寫入長度
                            ringBuffer_.enqueue(static_cast<size_t>(n));
                            cv_.notify_one();
                        }
                    }
                    catch (const Poco::Exception &e)
                    {
                        if (running_)
                            LOG_F(ERROR, "Socket error: %s", e.displayText().c_str());
                    }
                }
            }
            catch (const std::exception &e)
            {
                LOG_F(ERROR, "Thread exception: %s", e.what());
            }
            catch (...)
            {
                LOG_F(ERROR, "Unknown exception in thread");
            }
        }

        void consumer()
        {
            try
            {
                std::vector<char> tmp;
                tmp.reserve(sizeof(domain::FinancePackageMessage));

                while (running_)
                {
                    std::unique_lock<std::mutex> lk(cvMutex_);
                    cv_.wait(lk, [this]
                             { return !ringBuffer_.empty() || !running_; });

                    if (!running_)
                        break;

                    auto segOpt = ringBuffer_.getNextPacket();
                    if (!segOpt)
                    {
                        continue;
                    }
                    auto seg = *segOpt;

                    // 心跳過濾
                    if (seg.totalLen() < 3)
                    {
                        ringBuffer_.dequeue(seg.totalLen());
                        continue;
                    }

                    // 重組跨界封包
                    const domain::FinancePackageMessage *pkg;
                    if (seg.len2 == 0)
                    {
                        pkg = reinterpret_cast<const domain::FinancePackageMessage *>(seg.ptr1);
                    }
                    else
                    {
                        tmp.clear();
                        tmp.insert(tmp.end(), seg.ptr1, seg.ptr1 + seg.len1);
                        tmp.insert(tmp.end(), seg.ptr2, seg.ptr2 + seg.len2);
                        pkg = reinterpret_cast<const domain::FinancePackageMessage *>(tmp.data());
                    }

                    // 解析並 sync
                    auto res = handler_->processData(pkg->ap_data);
                    if (res.is_ok())
                    {
                        auto &s = res.unwrap();

                        const std::string key = "summary:" + s.area_center + ":" + s.stock_id;
                        auto r2 = repository_->sync(key, s);
                        if (r2.is_err())
                        {
                            LOG_F(ERROR, "Redis sync 失敗 key=%s: %s",
                                  key.c_str(), r2.unwrap_err().message.c_str());
                        }
                    }
                    else
                    {
                        LOG_F(ERROR, "封包解析失敗: %s", res.unwrap_err().message.c_str());
                    }

                    ringBuffer_.dequeue(seg.totalLen());
                }
            }
            catch (const std::exception &e)
            {
                LOG_F(ERROR, "Thread exception: %s", e.what());
            }
            catch (...)
            {
                LOG_F(ERROR, "Unknown exception in thread");
            }
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
} // namespace finance::infrastructure::network