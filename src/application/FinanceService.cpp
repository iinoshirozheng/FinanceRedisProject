// FinanceService.cpp
#include "FinanceService.h"
#include "../domain/Result.hpp"
#include "../infrastructure/config/ConnectionConfigProvider.hpp"
#include <csignal>
#include <thread>
#include <chrono>
#include <loguru.hpp>

using finance::domain::Result;

namespace finance::application
{
    // 全域指標，用於 signal callback
    static FinanceService *g_service = nullptr;

    FinanceService::FinanceService()
        : repository_(std::make_shared<infrastructure::storage::RedisSummaryAdapter>()),
          packetHandler_(std::make_unique<infrastructure::network::PacketProcessorFactory>(repository_))
    {
    }

    Result<void, std::string> FinanceService::initialize()
    {
        if (isInitialized_)
            return Result<void, std::string>::Err("Service 已初始化");

        // 1. 連線 Redis
        auto initRes = repository_->init();
        if (initRes.is_err())
            return Result<void, std::string>::Err("Redis init 失敗: " + initRes.unwrap_err().message);

        // 2. 載入所有快取
        LOG_F(INFO, "從 Redis 載入資料...");
        auto loadRes = repository_->loadAll();
        if (loadRes.is_err())
            return Result<void, std::string>::Err("Redis loadAll 失敗: " + loadRes.unwrap_err().message);
        LOG_F(INFO, "已載入 %zu 筆 summary 資料", repository_->getAll().size());

        // 3. 建立 TCP Service Adapter
        LOG_F(INFO, "啟動 TCP 服務 (port=%d)",
              infrastructure::config::ConnectionConfigProvider::serverPort());
        tcpService_ = std::make_unique<infrastructure::network::TcpServiceAdapter>(packetHandler_, repository_);

        isInitialized_ = true;
        return Result<void, std::string>::Ok();
    }

    Result<void, std::string> FinanceService::run()
    {
        if (!isInitialized_)
            return Result<void, std::string>::Err("Service 尚未初始化");
        if (isRunning_)
            return Result<void, std::string>::Err("Service 已在運行中");

        // 啟動 TCP 伺服器
        if (!tcpService_->start())
            return Result<void, std::string>::Err("TCP Service 啟動失敗");

        // 設置信號回呼
        g_service = this;
        std::signal(SIGINT, [](int sig)
                    { if (g_service) g_service->signalStatus_ = sig; });
        std::signal(SIGTERM, [](int sig)
                    { if (g_service) g_service->signalStatus_ = sig; });

        isRunning_ = true;
        LOG_F(INFO, "Finance System 運行中，按 Ctrl+C 停止");

        // 主循環：等待中斷／終止信號
        while (signalStatus_ == 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        return Result<void, std::string>::Ok();
    }

    void FinanceService::shutdown()
    {
        if (tcpService_)
            tcpService_->stop();

        isRunning_ = false;
        g_service = nullptr;
    }
}
