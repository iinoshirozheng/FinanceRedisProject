#pragma once

#include "domain/FinanceDataStructure.hpp"
#include "domain/Result.hpp"
#include "domain/IFinanceRepository.hpp"
#include <string>
#include <optional>
#include <functional>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>

namespace finance::infrastructure::tasks
{

    using finance::domain::ErrorResult;
    using finance::domain::Result;
    using finance::domain::SummaryData;

    // Define Redis operation types
    enum class RedisOperationType
    {
        SYNC_SUMMARY_DATA,
        UPDATE_COMPANY_SUMMARY,
        // Add other operation types as needed
    };

    // Redis task structure
    struct RedisTask
    {
        RedisOperationType operation;
        std::string key;                                                  // Key for data location (e.g., "summary:AREA:STOCK" or stock_id)
        std::optional<SummaryData> summary_data_payload;                  // Data for SYNC operations (copied by value to ensure lifetime)
        std::shared_ptr<std::promise<Result<void, ErrorResult>>> promise; // For async operation results

        // Add default constructor
        RedisTask() : operation(RedisOperationType::SYNC_SUMMARY_DATA), key(""), promise(nullptr) {}

        // Constructors
        RedisTask(RedisOperationType op, std::string k,
                  std::shared_ptr<std::promise<Result<void, ErrorResult>>> p)
            : operation(op), key(std::move(k)), promise(std::move(p)) {}

        RedisTask(RedisOperationType op, std::string k, SummaryData data,
                  std::shared_ptr<std::promise<Result<void, ErrorResult>>> p)
            : operation(op), key(std::move(k)), summary_data_payload(std::move(data)), promise(std::move(p)) {}
    };

    // Thread-safe task queue for Redis operations
    class RedisTaskQueue
    {
    public:
        void push(RedisTask task)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(task));
            cv_.notify_one();
        }

        bool try_pop(RedisTask &task)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty())
            {
                return false;
            }
            task = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        void wait_and_pop(RedisTask &task)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]
                     { return !queue_.empty(); });
            task = std::move(queue_.front());
            queue_.pop();
        }

        bool empty() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.empty();
        }

        size_t size() const
        {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.size();
        }

    private:
        std::queue<RedisTask> queue_;
        mutable std::mutex mutex_;
        std::condition_variable cv_;
    };

} // namespace finance::infrastructure::tasks