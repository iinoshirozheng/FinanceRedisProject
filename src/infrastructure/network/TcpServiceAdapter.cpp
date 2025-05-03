#include "TcpServiceAdapter.h"
#include "../../domain/FinanceDataStructure.h"
#include "../../../lib/loguru/loguru.hpp"

namespace finance::infrastructure::network
{
    using namespace std::chrono_literals;

    TcpServiceAdapter::TcpServiceAdapter(int port,
                                         std::shared_ptr<domain::IPackageHandler> handler)
        : serverSocket_{{"0.0.0.0", static_cast<unsigned short>(port)}}, handler_(std::move(handler)), ringBuffer_()
    {
        serverSocket_.setReuseAddress(true);
        serverSocket_.setReusePort(true);
    }

    TcpServiceAdapter::~TcpServiceAdapter()
    {
        stop();
    }

    bool TcpServiceAdapter::start()
    {
        if (running_)
            return false;
        running_ = true;
        acceptThread_ = std::thread(&TcpServiceAdapter::Producer, this);
        processingThread_ = std::thread(&TcpServiceAdapter::Consumer, this);
        return true;
    }

    void TcpServiceAdapter::stop()
    {
        if (!running_)
            return;
        running_ = false;
        serverSocket_.close(); // 讓 accept 退出
        // 也可在此加喚醒 Consumer 的機制，若 waitForData 改成帶超時就不必
        if (acceptThread_.joinable())
            acceptThread_.join();
        if (processingThread_.joinable())
            processingThread_.join();
    }

    // Producer: 只 Accept 一次，後續循環讀 socket→enqueue
    void TcpServiceAdapter::Producer()
    {
        Poco::Net::StreamSocket client;
        try
        {
            client = serverSocket_.acceptConnection();
            client.setReceiveTimeout(Poco::Timespan(SOCKET_TIMEOUT_MS * 1000));
            LOG_F(INFO, "Accepted client %s", client.peerAddress().toString());
        }
        catch (const std::exception &ex)
        {
            LOG_F(ERROR, "Accept failed: %s", ex.what());
            running_ = false;
            return;
        }

        while (running_)
        {
            // 1) 取得可寫空間指標
            size_t maxLen = 0;
            char *writePtr = ringBuffer_.writablePtr(maxLen);

            // 2) 空間不足就 spin 等
            if (maxLen == 0)
            {
                ringBuffer_.waitForSpace(1);
                continue;
            }

            // 3) 讀 socket
            int n = client.receiveBytes(writePtr, static_cast<int>(maxLen));
            if (n <= 0)
            {
                LOG_F(INFO, "Client disconnected");
                break;
            }

            // 4) 濾掉心跳
            if (n == 2 || n == 3)
            {
                LOG_F(INFO, "Keep-alive packet, len=%d", n);
                continue;
            }

            // 5) 入隊
            try
            {
                ringBuffer_.enqueue(static_cast<size_t>(n));
            }
            catch (const std::overflow_error &e)
            {
                LOG_F(ERROR, "RingBuffer overflow: %s", e.what());
                break;
            }
        }
    }

    // Consumer: wait→findPacket→peekFirst/Second→拼接→process→dequeue
    void TcpServiceAdapter::Consumer()
    {
        using Buffer = RingBuffer<RING_BUFFER_SIZE>;
        Buffer::PacketRef ref;
        std::vector<char> tmpBuf;
        tmpBuf.reserve(RING_BUFFER_SIZE);

        while (running_)
        {
            // 等待至少有一個完整封包
            ringBuffer_.waitForData();

            // 處理所有可取回的封包
            while (ringBuffer_.findPacket(ref))
            {
                tmpBuf.clear();

                // 第一段
                size_t firstLen;
                auto [ptr1, len1] = ringBuffer_.peekFirst(firstLen);

                if (firstLen >= ref.length)
                {
                    tmpBuf.insert(tmpBuf.end(), ptr1, ptr1 + ref.length);
                }
                else
                {
                    tmpBuf.insert(tmpBuf.end(), ptr1, ptr1 + firstLen);
                    // 第二段
                    size_t secondLen;
                    auto [ptr2, len2] = ringBuffer_.peekSecond(firstLen);
                    size_t need2 = ref.length - firstLen;
                    tmpBuf.insert(tmpBuf.end(), ptr2, ptr2 + need2);
                }

                // 處理
                auto fb = reinterpret_cast<const domain::FinancePackageMessage *>(tmpBuf.data());
                try
                {
                    handler_->processData(fb->ap_data);
                }
                catch (const std::exception &e)
                {
                    LOG_F(ERROR, "processData 錯誤：%s", e.what());
                }

                // 消費
                ringBuffer_.dequeue(ref.length);
            }
        }
    }
}
