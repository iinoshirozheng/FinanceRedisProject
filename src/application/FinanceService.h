// FinanceService.h
#pragma once

#include "../domain/Result.hpp"
#include "../infrastructure/network/TcpServiceAdapter.h"
#include "../infrastructure/storage/RedisSummaryAdapter.h"

#include <memory>
#include <atomic>
#include <string>

namespace finance::application
{
    /**
     * @brief 核心服務：負責 Redis 快取、TCP 伺服器啟動與信號控制
     */
    class FinanceService
    {
    public:
        FinanceService();
        ~FinanceService() = default;

        /**
         * @brief 初始化：連線 Redis、載入快取、建立 TCP Adapter
         * @return Result<void, std::string>
         *         - Ok(): 初始化成功
         *         - Err(msg): 初始化失敗，msg 為錯誤描述
         */
        finance::domain::Result<void, std::string> initialize();

        /**
         * @brief 啟動主循環：監聽並分派封包
         * @return Result<void, std::string>
         *         - Ok(): 服務正常結束
         *         - Err(msg): 運行失敗，msg 為錯誤描述
         */
        finance::domain::Result<void, std::string> run();

        /**
         * @brief 停止服務
         */
        void shutdown();

    private:
        std::shared_ptr<infrastructure::storage::RedisSummaryAdapter> repository_;
        std::shared_ptr<infrastructure::network::PacketProcessorFactory> packetHandler_;
        std::unique_ptr<infrastructure::network::TcpServiceAdapter> tcpService_;

        std::atomic<int> signalStatus_{0};
        bool isInitialized_{false};
        bool isRunning_{false};
    };
}
