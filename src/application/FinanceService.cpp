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
using finance::domain::IFinanceRepository;
using finance::domain::IPackageHandler;
using finance::domain::Result;
using finance::domain::SummaryData;

namespace finance::application
{
    // 全域指標，用於 signal callback
    static FinanceService *g_service = nullptr;
    static void (*g_signal_handler)(int) = nullptr;

    FinanceService::FinanceService(std::shared_ptr<IFinanceRepository<SummaryData, ErrorResult>> repository,
                                   std::shared_ptr<IPackageHandler> packetHandler)
        : repository_(std::move(repository)),
          packetHandler_(std::move(packetHandler))
    {
        // Initialize g_service for signal handling
        g_service = this;
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

        // 設置信號回調
        g_signal_handler = [](int sig)
        {
            LOG_F(WARNING, "處理信號: %d", sig);
            if (g_service)
            {
                g_service->signalStatus_ = sig;
                g_service->shutdown();
            }
        };
        std::signal(SIGINT, g_signal_handler);
        std::signal(SIGTERM, g_signal_handler);

        isRunning_ = true;
        LOG_F(INFO, "Finance System 運行中，按 Ctrl+C 停止");

        // 主循環：等待中斷／終止信號
        while (signalStatus_ == 0)
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
