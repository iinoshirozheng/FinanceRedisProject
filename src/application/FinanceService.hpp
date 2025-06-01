#pragma once

#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <future>
#include <csignal>
#include <loguru.hpp>
#include <functional>

#include "domain/IFinanceRepository.hpp"
#include "domain/IPackageHandler.hpp"
#include "domain/FinanceDataStructure.hpp"
#include "infrastructure/network/TcpServiceAdapter.hpp"
#include "infrastructure/config/ConnectionConfigProvider.hpp"
#include "infrastructure/tasks/RedisTask.hpp"
#include "infrastructure/tasks/RedisWorker.hpp"
#include "infrastructure/storage/RedisSummaryAdapter.hpp"

namespace finance::application
{
    using finance::domain::ErrorCode;
    using finance::domain::ErrorResult;
    using finance::domain::Result;
    using finance::domain::SummaryData;
    using finance::infrastructure::tasks::RedisOperationType;
    using finance::infrastructure::tasks::RedisTask;
    using finance::infrastructure::tasks::RedisWorker;

    class FinanceService
    {
    public:
        explicit FinanceService(
            std::shared_ptr<finance::domain::IFinanceRepository<SummaryData, ErrorResult>> repo,
            std::shared_ptr<finance::domain::IPackageHandler> handler)
            : repository_(std::move(repo)), processor_(std::move(handler)) {}

        ~FinanceService() = default;

        // src/application/FinanceService.hpp
        Result<void, ErrorResult> initialize()
        {
            try
            {
                // 第一步：初始化 Repository (例如，建立 Redis 連線)
                if (!repository_)
                {
                    LOG_F(ERROR, "FinanceService::initialize: Repository is null.");
                    return Result<void, ErrorResult>::Err(
                        ErrorResult{ErrorCode::InternalError, "Repository is null during service initialization"});
                }

                LOG_F(INFO, "FinanceService::initialize: Initializing repository...");
                auto repoInitResult = repository_->init(); // <<<< 新增呼叫 repository 的 init() 方法
                if (repoInitResult.is_err())
                {
                    LOG_F(ERROR, "FinanceService::initialize: Repository initialization failed: %s", repoInitResult.unwrap_err().message.c_str());
                    // 將底層的錯誤直接透傳出去
                    return Result<void, ErrorResult>::Err(repoInitResult.unwrap_err());
                }
                LOG_F(INFO, "FinanceService::initialize: Repository initialized successfully.");

                LOG_F(INFO, "FinanceService::initialize: Loading all data from repository...");
                auto loadAllResult = repository_->loadAll();
                if (loadAllResult.is_err())
                {
                    LOG_F(ERROR, "FinanceService::initialize: Failed to load all data: %s", loadAllResult.unwrap_err().message.c_str());
                    // 根據你的業務邏輯，這裡可以是致命錯誤，也可以只是一個警告然後繼續
                    // 例如，如果快取只是為了效能，即使載入失敗，服務或許仍能運作（只是較慢）
                    // 如果初始資料是必須的，則應該返回錯誤：
                    return Result<void, ErrorResult>::Err(loadAllResult.unwrap_err());
                }
                else
                {
                    LOG_F(INFO, "FinanceService::initialize: All data loaded from repository successfully (or no data to load).");
                }

                // 第二步：創建並啟動 Redis worker (它依賴於已初始化的 repository)
                LOG_F(INFO, "FinanceService::initialize: Creating and starting RedisWorker...");
                redis_worker_ = std::make_unique<RedisWorker>(repository_);
                redis_worker_->start();
                LOG_F(INFO, "FinanceService::initialize: RedisWorker started.");

                // 設定 Task Submitter 給 RedisAdapter
                LOG_F(INFO, "FinanceService::initialize: Setting up task submitter for RedisAdapter...");
                auto submitter = [this](RedisTask task)
                {
                    return this->submitRedisTask(std::move(task));
                };

                auto redis_adapter = std::dynamic_pointer_cast<infrastructure::storage::RedisSummaryAdapter>(repository_);
                if (redis_adapter)
                {
                    redis_adapter->setTaskSubmitter(submitter);
                    LOG_F(INFO, "FinanceService::initialize: Task submitter set for RedisAdapter.");
                }
                else
                {
                    LOG_F(WARNING, "FinanceService::initialize: Repository is not a RedisSummaryAdapter, cannot set task submitter directly.");
                }

                // 第三步：創建 TCP 服務適配器
                LOG_F(INFO, "FinanceService::initialize: Creating TcpServiceAdapter...");
                tcp_adapter_ = std::make_shared<infrastructure::network::TcpServiceAdapter>(processor_, repository_);
                LOG_F(INFO, "FinanceService::initialize: TcpServiceAdapter created.");

                LOG_F(INFO, "FinanceService::initialize: Initialization complete.");
                return Result<void, ErrorResult>::Ok();
            }
            catch (const std::exception &e)
            {
                LOG_F(ERROR, "FinanceService::initialize: Exception caught during initialization: %s", e.what());
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InternalError,
                                "Failed to initialize service: " + std::string(e.what())});
            }
        }

        Result<void, ErrorResult> run()
        {
            if (!tcp_adapter_)
            {
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InternalError, "Service not initialized"});
            }

            try
            {
                if (!tcp_adapter_->start())
                {
                    return Result<void, ErrorResult>::Err(
                        ErrorResult{ErrorCode::InternalError, "Failed to start TCP adapter"});
                }
                return Result<void, ErrorResult>::Ok();
            }
            catch (const std::exception &e)
            {
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InternalError,
                                "Failed to start service: " + std::string(e.what())});
            }
        }

        void wait()
        {
            if (tcp_adapter_)
            {
                tcp_adapter_->wait();
            }
        }

        std::future<Result<void, ErrorResult>> submitRedisTask(RedisTask task)
        {
            if (!redis_worker_)
            {
                std::promise<Result<void, ErrorResult>> promise;
                promise.set_value(Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InternalError, "Redis worker not initialized"}));
                return promise.get_future();
            }
            return redis_worker_->submit_task(std::move(task));
        }

        std::shared_ptr<finance::domain::IFinanceRepository<SummaryData, ErrorResult>> getRepository() const
        {
            return repository_;
        }

    private:
        std::unique_ptr<RedisWorker> redis_worker_;
        std::shared_ptr<finance::domain::IFinanceRepository<SummaryData, ErrorResult>> repository_;
        std::shared_ptr<finance::domain::IPackageHandler> processor_;
        std::shared_ptr<infrastructure::network::TcpServiceAdapter> tcp_adapter_;
    };

    static FinanceService *g_service = nullptr;

} // namespace finance::application
