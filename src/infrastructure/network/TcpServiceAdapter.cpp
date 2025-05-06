// infrastructure/network/TcpServiceAdapter.cpp
#include "TcpServiceAdapter.h"
#include <loguru.hpp>

namespace finance::infrastructure::network
{
    using domain::Result;
    using domain::ResultError;
    using utils::FinanceUtils;

    void TcpServiceAdapter::consumer()
    {
        std::vector<char> tmp;
        tmp.reserve(RING_BUFFER_SIZE);

        while (running_)
        {
            auto segOpt = ringBuffer_.getNextPacket();
            if (!segOpt)
            {
                cpu_pause();
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
