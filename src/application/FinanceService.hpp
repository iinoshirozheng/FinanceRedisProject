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

        Result<void, ErrorResult> initialize()
        {
            try
            {
                // Create Redis worker first
                redis_worker_ = std::make_unique<RedisWorker>(repository_);
                redis_worker_->start();

                // Create task submitter lambda
                auto submitter = [this](RedisTask task)
                {
                    return this->submitRedisTask(std::move(task));
                };

                // Create Redis repository with task submitter
                auto redis_adapter = std::dynamic_pointer_cast<infrastructure::storage::RedisSummaryAdapter>(repository_);
                if (redis_adapter)
                {
                    redis_adapter->setTaskSubmitter(submitter);
                }

                // Create TCP service adapter with processor and repository
                tcp_adapter_ = std::make_shared<infrastructure::network::TcpServiceAdapter>(processor_, repository_);

                return Result<void, ErrorResult>::Ok();
            }
            catch (const std::exception &e)
            {
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
