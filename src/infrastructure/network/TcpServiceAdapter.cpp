// infrastructure/network/TcpServiceAdapter.cpp
#include "TcpServiceAdapter.h"
#include <loguru.hpp>
#include <Poco/Net/StreamSocket.h>

namespace finance::infrastructure::network
{
    using domain::ErrorResult;
    using domain::Result;
    using utils::FinanceUtils;

    TcpServiceAdapter::~TcpServiceAdapter()
    {
        stop();
    }

    bool TcpServiceAdapter::start()
    {
        if (running_)
            return false;

        running_ = true;
        acceptThread_ = std::thread(&TcpServiceAdapter::producer, this);
        processingThread_ = std::thread(&TcpServiceAdapter::consumer, this);
        return true;
    }

    void TcpServiceAdapter::stop()
    {
        if (!running_)
            return;

        running_ = false;
        if (acceptThread_.joinable())
            acceptThread_.join();
        if (processingThread_.joinable())
            processingThread_.join();
    }

    void TcpServiceAdapter::producer()
    {
        Poco::Net::StreamSocket clientSocket;
        std::vector<char> buffer(RING_BUFFER_SIZE);

        while (running_)
        {
            try
            {
                clientSocket = serverSocket_.acceptConnection();
                clientSocket.setReceiveTimeout(Poco::Timespan(SOCKET_TIMEOUT_MS * 1000));

                while (running_)
                {
                    int n = clientSocket.receiveBytes(buffer.data(), buffer.size());
                    if (n <= 0)
                        break;

                    size_t maxLen;
                    char *writePtr = ringBuffer_.writablePtr(maxLen);
                    if (static_cast<size_t>(n) > maxLen)
                    {
                        LOG_F(ERROR, "Buffer overflow: received %d bytes but only %zu available", n, maxLen);
                        break;
                    }
                    std::memcpy(writePtr, buffer.data(), n);
                    ringBuffer_.enqueue(n);
                }
            }
            catch (const Poco::Exception &e)
            {
                if (running_)
                    LOG_F(ERROR, "Socket error: %s", e.displayText().c_str());
            }
        }
    }

    void TcpServiceAdapter::consumer()
    {
        std::vector<char> tmp;
        tmp.reserve(RING_BUFFER_SIZE);

        while (running_)
        {
            auto segOpt = ringBuffer_.getNextPacket();
            if (!segOpt)
            {
                std::this_thread::yield();
                continue;
            }
            auto seg = *segOpt;

            // 心跳過濾
            if (seg.totalLen() == 2 || seg.totalLen() == 3)
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
                auto key = FinanceUtils::generateKey("summary", s);
                auto r2 = repository_->sync(s);
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
}
