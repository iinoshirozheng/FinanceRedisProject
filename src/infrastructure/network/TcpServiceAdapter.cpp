#include "TcpServiceAdapter.h"
#include "../../domain/FinanceDataStructure.h"
#include <Poco/Net/StreamSocket.h>
#include <Poco/Exception.h>
#include "../../../lib/loguru/loguru.hpp"

namespace finance::infrastructure::network
{
    using namespace std::chrono_literals;

    TcpServiceAdapter::~TcpServiceAdapter()
    {
        stop();
    }

    bool TcpServiceAdapter::start()
    {
        if (running_)
        {
            return false;
        }

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
        // 先關閉 listener，使 acceptConnection() 跳出
        serverSocket_.close();
        // 再等待兩條執行緒結束
        if (acceptThread_.joinable())
            acceptThread_.join();
        if (processingThread_.joinable())
            processingThread_.join();
    }

    void TcpServiceAdapter::Producer()
    {
        // 持續接收新連線
        while (running_)
        {
            Poco::Net::StreamSocket client;
            try
            {
                // 接受客戶端連線
                client = serverSocket_.acceptConnection();
                client.setReceiveTimeout(Poco::Timespan(SOCKET_TIMEOUT_MS * 1000));
                LOG_F(INFO, "已接受客戶端：%s", client.peerAddress().toString().c_str());
            }
            catch (const Poco::Exception &ex)
            {
                if (running_)
                    LOG_F(ERROR, "Accept 失敗：%s", ex.displayText().c_str());
                break; // listener 已關閉或發生錯誤，跳出迴圈
            }

            // 讀取該客戶端的資料並寫入環形緩衝區
            while (running_)
            {
                // 1) 取得可寫指標及最大可寫長度
                size_t maxLen = 0;
                char *writePtr = ringBuffer_.writablePtr(maxLen);

                // 2) 若無可用空間，等待後重試
                if (maxLen == 0)
                {
                    ringBuffer_.waitForSpace(1);
                    continue;
                }

                int n = 0;
                try
                {
                    // 從 socket 接收資料
                    n = client.receiveBytes(writePtr, static_cast<int>(maxLen));
                }
                catch (const Poco::TimeoutException &)
                {
                    // 超時僅代表暫無資料，繼續下一輪
                    continue;
                }
                catch (const Poco::Exception &ex)
                {
                    LOG_F(ERROR, "接收資料錯誤：%s", ex.displayText().c_str());
                    break;
                }

                // 3) 客戶端斷線
                if (n <= 0)
                {
                    LOG_F(INFO, "客戶端已斷線");
                    break;
                }

                // 4) 真正入隊
                try
                {
                    ringBuffer_.enqueue(static_cast<size_t>(n));
                }
                catch (const std::overflow_error &e)
                {
                    LOG_F(ERROR, "RingBuffer overflow : %s", e.what());
                    break;
                }
            }
            // 當前客戶端處理結束，回到外層迴圈等待下一個連線
        }
    }

    void TcpServiceAdapter::Consumer()
    {
        std::vector<char> tmpBuf;
        tmpBuf.reserve(RING_BUFFER_SIZE); // 只做一次記憶體配置

        while (running_)
        {
            auto segOpt = ringBuffer_.getNextPacket();
            if (!segOpt)
            {
                cpu_pause();
                continue;
            }
            auto seg = *segOpt;

            // 過濾心跳包（長度為 2 或 3）
            size_t totalLen = seg.totalLen();
            if (totalLen == 2 || totalLen == 3)
            {
                LOG_F(INFO, "心跳包，長度=%zu", totalLen);
                ringBuffer_.dequeue(totalLen);
                continue;
            }

            if (seg.len2 == 0)
            {
                // 整包都在連續一段
                auto fb = reinterpret_cast<const domain::FinancePackageMessage *>(seg.ptr1);
                try
                {
                    handler_->processData(fb->ap_data);
                }
                catch (const std::exception &e)
                {
                    LOG_F(ERROR, "processData 發生例外：%s", e.what());
                }
            }
            else
            {
                // 跨界，才做一次拼接
                tmpBuf.clear();
                tmpBuf.insert(tmpBuf.end(), seg.ptr1, seg.ptr1 + seg.len1);
                tmpBuf.insert(tmpBuf.end(), seg.ptr2, seg.ptr2 + seg.len2);
                auto fb = reinterpret_cast<const domain::FinancePackageMessage *>(tmpBuf.data());
                try
                {
                    handler_->processData(fb->ap_data);
                }
                catch (const std::exception &e)
                {
                    LOG_F(ERROR, "processData 發生例外：%s", e.what());
                }
            }

            // 最後移除已處理的 bytes
            ringBuffer_.dequeue(seg.totalLen());
        }
    }
}
