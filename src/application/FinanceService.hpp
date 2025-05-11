#pragma once

#include "../domain/Result.hpp"
#include "../domain/IFinanceRepository.hpp"
#include "../domain/IPackageHandler.hpp"
#include "../infrastructure/network/TcpServiceAdapter.hpp"
#include "../infrastructure/config/ConnectionConfigProvider.hpp"
#include <memory>
#include <atomic>
#include <csignal>
#include <thread>
#include <chrono>
#include <loguru.hpp>

namespace finance::application
{
    /**
     * @brief FinanceService 負責啟動、管理 TCP 服務，以及注入相應的處理邏輯
     */
    class FinanceService
    {
    public:
        /**
         * @brief 取得 singleton instance
         */
        static FinanceService *instance();

        /**
         * @brief 初始化 FinanceService
         * @param repo 注入的資料儲存庫
         * @param configProvider 連線設定提供者
         */
        void init(std::shared_ptr<domain::IFinanceRepository> repo,
                  std::shared_ptr<config::ConnectionConfigProvider> configProvider);

        /**
         * @brief 啟動服務
         */
        Result<void, ErrorResult> start();

        /**
         * @brief 停止服務
         */
        void shutdown();

    private:
        FinanceService();
        ~FinanceService() = default;

        FinanceService(const FinanceService &) = delete;
        FinanceService &operator=(const FinanceService &) = delete;

        std::shared_ptr<domain::IFinanceRepository> repository_{nullptr};
        std::shared_ptr<domain::IPackageHandler> handler_{nullptr};
        std::shared_ptr<infrastructure::network::TcpServiceAdapter> tcpService_{nullptr};
        std::shared_ptr<config::ConnectionConfigProvider> configProvider_{nullptr};

        std::atomic<bool> isRunning_{false};

        static FinanceService *g_service;
    };

    // inline 實作

    inline FinanceService *FinanceService::instance()
    {
        if (!g_service)
        {
            g_service = new FinanceService();
        }
        return g_service;
    }

    inline FinanceService::FinanceService()
    {
        // 預設建構，可留空
    }

    inline void FinanceService::init(std::shared_ptr<domain::IFinanceRepository> repo,
                                     std::shared_ptr<config::ConnectionConfigProvider> configProvider)
    {
        repository_ = std::move(repo);
        configProvider_ = std::move(configProvider);
    }

    inline Result<void, ErrorResult> FinanceService::start()
    {
        if (isRunning_)
        {
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::ServiceAlreadyRunning, "Service 已在執行中"});
        }

        // 設置 Ctrl+C 處理
        std::signal(SIGINT, [](int)
                    { FinanceService::instance()->shutdown(); });

        // 讀取設定
        auto cfg = configProvider_->getConfig();
        if (cfg.is_err())
        {
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::ConfigLoadFailed, cfg.unwrap_err().message});
        }

        // 建立 handler 並注入 repository
        handler_ = std::make_shared<domain::IPackageHandler>(repository_);

        // 建立並啟動 TCP 服務
        tcpService_ = std::make_shared<infrastructure::network::TcpServiceAdapter>(handler_, cfg.unwrap());
        if (!tcpService_->start())
        {
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::TcpServiceStartFailed, "無法啟動 TCP 服務"});
        }

        isRunning_ = true;
        LOG_F(INFO, "Finance Service 已啟動");
        // 等待直到 shutdown 被呼叫
        while (isRunning_)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // 正常關閉
        shutdown();
        return Result<void, ErrorResult>::Ok();
    }

    inline void FinanceService::shutdown()
    {
        if (tcpService_)
        {
            tcpService_->stop();
        }
        isRunning_ = false;
        g_service = nullptr;
        LOG_F(INFO, "Finance Service 已關閉");
    }
}
