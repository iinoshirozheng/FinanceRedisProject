#pragma once

#include "infrastructure/tasks/RedisTask.hpp"
#include "domain/IFinanceRepository.hpp"
#include <memory>
#include <thread>
#include <atomic>

namespace finance::infrastructure::tasks
{
    using finance::domain::ErrorCode;
    using finance::domain::ErrorResult;
    using finance::domain::Result;
    using finance::domain::SummaryData;
    using finance::infrastructure::tasks::RedisTask;

    class RedisWorker
    {
    public:
        explicit RedisWorker(std::shared_ptr<finance::domain::IFinanceRepository<finance::domain::SummaryData, finance::domain::ErrorResult>> repository)
            : repository_(std::move(repository)), running_(false) {}

        ~RedisWorker()
        {
            stop();
        }

        // Start the worker thread
        void start()
        {
            if (running_.load())
            {
                return;
            }
            running_ = true;
            worker_thread_ = std::thread(&RedisWorker::process_tasks, this);
        }

        // Stop the worker thread
        void stop()
        {
            if (!running_.exchange(false))
            {
                return;
            }
            if (worker_thread_.joinable())
            {
                worker_thread_.join();
            }
        }

        // Submit a task to the queue
        std::future<Result<void, ErrorResult>> submit_task(RedisTask task)
        {
            auto promise = std::make_shared<std::promise<Result<void, ErrorResult>>>();
            task.promise = promise;
            task_queue_.push(std::move(task));
            return promise->get_future();
        }

    private:
        void process_tasks()
        {
            while (running_.load())
            {
                RedisTask task;
                task_queue_.wait_and_pop(task);

                if (!running_.load())
                {
                    break;
                }

                try
                {
                    switch (task.operation)
                    {
                    case RedisOperationType::SYNC_SUMMARY_DATA:
                        if (task.summary_data_payload)
                        {
                            auto result = repository_->sync(task.key, &task.summary_data_payload.value());
                            task.promise->set_value(result);
                        }
                        else
                        {
                            task.promise->set_value(Result<void, ErrorResult>::Err(ErrorResult{ErrorCode::UnexpectedError, "Missing summary data payload"}));
                        }
                        break;

                    case RedisOperationType::UPDATE_COMPANY_SUMMARY:
                        task.promise->set_value(Result<void, ErrorResult>::Err(ErrorResult{ErrorCode::UnexpectedError, "Operation not implemented"}));
                        break;

                    default:
                        task.promise->set_value(Result<void, ErrorResult>::Err(ErrorResult{ErrorCode::UnexpectedError, "Unknown operation type"}));
                        break;
                    }
                }
                catch (const std::exception &e)
                {
                    task.promise->set_value(Result<void, ErrorResult>::Err(ErrorResult{ErrorCode::UnexpectedError, std::string("Exception in Redis worker: ") + e.what()}));
                }
            }
        }

        std::shared_ptr<finance::domain::IFinanceRepository<finance::domain::SummaryData, finance::domain::ErrorResult>> repository_;
        RedisTaskQueue task_queue_;
        std::thread worker_thread_;
        std::atomic<bool> running_;
    };

} // namespace finance::infrastructure::tasks