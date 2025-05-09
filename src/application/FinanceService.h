#pragma once

#include "../domain/Result.hpp"
#include "../domain/IFinanceRepository.h"
#include "../domain/IPackageHandler.h"
#include "../infrastructure/network/TcpServiceAdapter.h"

#include <memory>
#include <atomic>
#include <string>

// Forward declarations
namespace finance::infrastructure::storage
{
    class RedisSummaryAdapter;
}

namespace finance::infrastructure::network
{
    class PacketProcessorFactory;
}

namespace finance::application
{
    /**
     * @brief 核心服務：負責 Redis 快取、TCP 伺服器啟動與信號控制
     */

    class FinanceService
    {
    public:
        explicit FinanceService(std::shared_ptr<finance::domain::IFinanceRepository<finance::domain::SummaryData, finance::domain::ErrorResult>> repository,
                                std::shared_ptr<finance::domain::IPackageHandler> packetHandler);
        ~FinanceService() = default;

        /**
         * @brief 初始化：連線 Redis、載入快取、建立 TCP Adapter
         * @return Result<void>
         *         - Ok(): 初始化成功
         *         - Err(msg): 初始化失敗，msg 為錯誤描述
         */
        finance::domain::Result<void, finance::domain::ErrorResult> initialize();

        /**
         * @brief 啟動主循環：監聽並分派封包
         * @return Result<void>
         *         - Ok(): 服務正常結束
         *         - Err(msg): 運行失敗，msg 為錯誤描述
         */
        finance::domain::Result<void, finance::domain::ErrorResult> run();

        /**
         * @brief 停止服務
         */
        void shutdown();

    private:
        std::shared_ptr<finance::infrastructure::storage::RedisSummaryAdapter> repository_;
        std::shared_ptr<finance::infrastructure::network::PacketProcessorFactory> packetHandler_;
        std::unique_ptr<finance::infrastructure::network::TcpServiceAdapter> tcpService_;

        std::atomic<int> signalStatus_{0};
        bool isInitialized_{false};
        bool isRunning_{false};
    };
}
