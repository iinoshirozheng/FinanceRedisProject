#include "PacketQueue.h"
#include <chrono>

namespace finance
{
    namespace infrastructure
    {
        namespace network
        {
            using namespace std::chrono_literals;

            PacketQueue::PacketQueue() : running_(false)
            {
            }

            PacketQueue::~PacketQueue()
            {
                stopProcessing();

                // 清理所有剩餘的數據包
                std::lock_guard<std::mutex> lock(queueMutex_);
                while (!queue_.empty())
                {
                    char *data = queue_.front();
                    queue_.pop();
                    delete[] data;
                }
            }

            void PacketQueue::enqueue(char *data)
            {
                std::lock_guard<std::mutex> lock(queueMutex_);
                queue_.push(data);
                queueCondition_.notify_one();
            }

            bool PacketQueue::tryDequeue(char *&data)
            {
                std::unique_lock<std::mutex> lock(queueMutex_);

                if (!queueCondition_.wait_for(lock, 1ms, [this]
                                              { return !queue_.empty(); }))
                {
                    return false;
                }

                data = queue_.front();
                queue_.pop();
                return true;
            }

            void PacketQueue::startProcessing(std::function<void(char *)> processingFunction)
            {
                if (running_)
                {
                    return;
                }

                running_ = true;
                processingThread_ = std::thread(&PacketQueue::processPackets, this, processingFunction);
            }

            void PacketQueue::stopProcessing()
            {
                if (!running_)
                {
                    return;
                }

                running_ = false;
                queueCondition_.notify_all();

                if (processingThread_.joinable())
                {
                    processingThread_.join();
                }
            }

            void PacketQueue::processPackets(std::function<void(char *)> processingFunction)
            {
                while (running_)
                {
                    char *data = nullptr;
                    if (tryDequeue(data))
                    {
                        processingFunction(data);
                        delete[] data;
                    }
                }
            }

        } // namespace network
    } // namespace infrastructure
} // namespace finance