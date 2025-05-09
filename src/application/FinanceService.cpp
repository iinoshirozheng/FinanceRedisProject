// FinanceService.cpp
#include "FinanceService.h"
#include "../domain/Result.hpp"
#include "../infrastructure/config/ConnectionConfigProvider.hpp"
#include "../infrastructure/network/TcpServiceAdapter.h"
#include "../infrastructure/storage/RedisSummaryAdapter.h"
#include <csignal>
#include <thread>
#include <chrono>
#include <loguru.hpp>

using finance::domain::ErrorCode;
using finance::domain::ErrorResult;
using finance::domain::Result;

namespace finance::application
{
    // 全域指標，用於 signal callback
    static FinanceService *g_service = nullptr;

    FinanceService::FinanceService(std::shared_ptr<finance::domain::IFinanceRepository<finance::domain::SummaryData, finance::domain::ErrorResult>> repository,
                                   std::shared_ptr<finance::domain::IPackageHandler> packetHandler)
    {
        // Initialize members
    }

    Result<void, ErrorResult> FinanceService::initialize()
    {
        if (isInitialized_)
            return Result<void, ErrorResult>::Err(ErrorResult{ErrorCode::InternalError, "Service 已初始化"});

        // Use .map_err to transform errors, for void Results we need a lambda with no parameters
        auto result = repository_->init();
        if (result.is_err())
        {
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::InternalError, "FinanceService 初始化失敗: " + result.unwrap_err().message});
        }

        LOG_F(INFO, "從 Redis 載入資料...");
        auto loadResult = repository_->loadAll();
        if (loadResult.is_err())
        {
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::InternalError, "FinanceService 載入資料失敗: " + loadResult.unwrap_err().message});
        }

        LOG_F(INFO, "啟動 TCP 服務 (port=%d)", infrastructure::config::ConnectionConfigProvider::serverPort());
        tcpService_ = std::make_unique<infrastructure::network::TcpServiceAdapter>(packetHandler_, repository_);
        isInitialized_ = true;
        return Result<void, ErrorResult>::Ok();
    }

    Result<void, ErrorResult> FinanceService::run()
    {
        if (!isInitialized_)
            return Result<void, ErrorResult>::Err(ErrorResult{ErrorCode::InternalError, "Finance Service 尚未初始化"});
        if (isRunning_)
            return Result<void, ErrorResult>::Err(ErrorResult{ErrorCode::InternalError, "Finance Service 已在運行中"});

        // 啟動 TCP 服務
        if (!tcpService_->start())
            return Result<void, ErrorResult>::Err(ErrorResult{ErrorCode::TcpStartFailed, "TCP Service 啟動失敗"});

        // 設置信號回調 (使用 lambda 和 std::atomic)
        std::atomic<int> signalStatus{0};
        auto signalHandler = [](int sig)
        {
            LOG_F(WARNING, "處理信號: %d", sig);
            if (g_service)
            {
                g_service->shutdown();
            }
        };

        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        isRunning_ = true;
        LOG_F(INFO, "Finance System 運行中，按 Ctrl+C 停止");

        // 主循環：等待中斷／終止信號
        while (signalStatus == 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        shutdown(); // 收到信號後執行關閉邏輯
        return Result<void, ErrorResult>::Ok();
    }

    void FinanceService::shutdown()
    {
        if (tcpService_)
            tcpService_->stop();

        isRunning_ = false;
        g_service = nullptr;
    }
}
