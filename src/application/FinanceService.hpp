#pragma once

#include "domain/Result.hpp"
#include "domain/IFinanceRepository.hpp"
#include "domain/IPackageHandler.hpp"
#include "infrastructure/network/TcpServiceAdapter.hpp"
#include "infrastructure/config/ConnectionConfigProvider.hpp"
#include <memory>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#include <loguru.hpp>

namespace finance::application
{
    using finance::domain::ErrorCode;
    using finance::domain::ErrorResult;
    using finance::domain::Result;
    using finance::domain::SummaryData;

    class FinanceService; // Forward declaration

    // 全域指標，用於 signal callback
    static FinanceService *g_service = nullptr;

    class FinanceService
    {
    public:
        FinanceService(
            std::shared_ptr<finance::domain::IFinanceRepository<SummaryData, ErrorResult>> repository,
            std::shared_ptr<finance::domain::IPackageHandler> packetHandler)
            : repository_(std::move(repository)),
              packetHandler_(std::move(packetHandler))
        {
            // Initialize g_service for signal handling
            g_service = this;
        }

        Result<void, ErrorResult> initialize()
        {
            if (isInitialized_)
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InternalError, "Service 已初始化"});

            // 初始化 Redis 與載入快取
            auto initRes = repository_->init();
            if (initRes.is_err())
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InternalError, "FinanceService 初始化失敗: " + initRes.unwrap_err().message});
            LOG_F(INFO, "Redis 連線成功");

            LOG_F(INFO, "從 Redis 載入所有 summary...");
            auto loadRes = repository_->loadAll();
            if (loadRes.is_err())
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InternalError, "FinanceService 載入資料失敗: " + loadRes.unwrap_err().message});

            // 建立 TCP Service
            LOG_F(INFO, "啟動 TCP 服務 (port=%d)",
                  infrastructure::config::ConnectionConfigProvider::serverPort());
            tcpService_ = std::make_unique<infrastructure::network::TcpServiceAdapter>(
                packetHandler_, repository_);

            isInitialized_ = true;
            return Result<void, ErrorResult>::Ok();
        }

        Result<void, ErrorResult> run()
        {
            if (!isInitialized_)
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InternalError, "Finance Service 尚未初始化"});
            if (isRunning_)
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InternalError, "Finance Service 已在運行中"});

            // 啟動 TCP 服務
            if (!tcpService_->start())
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::TcpStartFailed, "TCP Service 啟動失敗"});

            // 設置信號回調
            auto signalHandler = [](int sig)
            {
                LOG_F(WARNING, "收到信號: %d", sig);
                if (g_service)
                {
                    g_service->signalStatus_ = sig; // 設定成員變數
                    g_service->shutdown();
                }
            };
            std::signal(SIGINT, signalHandler);
            std::signal(SIGTERM, signalHandler);

            isRunning_ = true;
            LOG_F(INFO, "Finance System 運行中，按 Ctrl+C 停止");

            // 主循環：等待 signalStatus_ 變更
            while (signalStatus_ == 0)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            // 離開前再確保關閉
            shutdown();
            return Result<void, ErrorResult>::Ok();
        }

        void shutdown()
        {
            if (tcpService_)
            {
                tcpService_->stop();
            }
            isRunning_ = false;
            g_service = nullptr;
            LOG_F(INFO, "Finance Service 已關閉");
        }

    private:
        std::shared_ptr<finance::domain::IFinanceRepository<SummaryData, ErrorResult>> repository_;
        std::shared_ptr<finance::domain::IPackageHandler> packetHandler_;
        std::unique_ptr<infrastructure::network::TcpServiceAdapter> tcpService_;

        bool isInitialized_{false};
        bool isRunning_{false};
        std::atomic<int> signalStatus_{0};
    };
} // namespace finance::application