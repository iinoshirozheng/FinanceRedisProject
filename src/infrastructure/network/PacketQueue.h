#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <chrono>

namespace finance
{
    namespace infrastructure
    {
        namespace network
        {
            /**
             * 用於管理數據包處理隊列的類
             */
            class PacketQueue
            {
            public:
                PacketQueue();
                ~PacketQueue();

                /**
                 * 將數據包添加到隊列
                 * @param data 數據包數據的指針（所有權被轉移）
                 */
                void enqueue(char *data);

                /**
                 * 嘗試從隊列中取出數據包
                 * @param data 接收數據包數據的指針引用
                 * @return 如果取出了數據包則返回真，如果隊列為空則返回假
                 */
                bool tryDequeue(char *&data);

                /**
                 * 啟動數據包處理線程
                 * @param processingFunction 處理每個數據包的函數
                 */
                void startProcessing(std::function<void(char *)> processingFunction);

                /**
                 * 停止數據包處理線程
                 */
                void stopProcessing();

            private:
                void processPackets(std::function<void(char *)> processingFunction);
                std::queue<char *> queue_;
                std::mutex queueMutex_;
                std::condition_variable queueCondition_;
                std::atomic<bool> running_;
                std::thread processingThread_;
            };

            template <typename T>
            class BoundedQueue
            {
            public:
                explicit BoundedQueue(size_t maxSize = 1000) : maxSize_(maxSize), closed_(false) {}

                ~BoundedQueue() { close(); }

                // Push with timeout
                bool push(const T &item, std::chrono::milliseconds timeout = std::chrono::milliseconds(100))
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    if (closed_)
                        return false;

                    auto success = notFull_.wait_for(lock, timeout, [this]
                                                     { return queue_.size() < maxSize_ || closed_; });

                    if (!success || closed_)
                        return false;

                    queue_.push(item);
                    notEmpty_.notify_one();
                    return true;
                }

                // Pop with timeout
                std::optional<T> pop(std::chrono::milliseconds timeout = std::chrono::milliseconds(100))
                {
                    std::unique_lock<std::mutex> lock(mutex_);
                    if (closed_)
                        return std::nullopt;

                    auto success = notEmpty_.wait_for(lock, timeout, [this]
                                                      { return !queue_.empty() || closed_; });

                    if (!success || closed_)
                        return std::nullopt;

                    T item = std::move(queue_.front());
                    queue_.pop();
                    notFull_.notify_one();
                    return item;
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

                void close()
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    if (!closed_)
                    {
                        closed_ = true;
                        notEmpty_.notify_all();
                        notFull_.notify_all();
                    }
                }

            private:
                std::queue<T> queue_;
                const size_t maxSize_;
                mutable std::mutex mutex_;
                std::condition_variable notEmpty_;
                std::condition_variable notFull_;
                bool closed_;
            };

        } // namespace network
    } // namespace infrastructure
} // namespace finance