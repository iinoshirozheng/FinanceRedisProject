#include "PacketQueue.h"
#include <chrono>

namespace finance::infrastructure::network
{
    using namespace std::chrono_literals;

    PacketQueue::PacketQueue() : running_(false)
    {
    }

    PacketQueue::~PacketQueue()
    {
        stopProcessing();
    }

    bool PacketQueue::enqueue(std::vector<char> &&data)
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (queue_.size() >= MAX_QUEUE_SIZE)
        {
            return false;
        }
        queue_.push(std::move(data));
        queueCondition_.notify_one();
        return true;
    }

    bool PacketQueue::tryDequeue(std::vector<char> &data)
    {
        std::unique_lock<std::mutex> lock(queueMutex_);

        if (!queueCondition_.wait_for(lock, 1ms, [this]
                                      { return !queue_.empty(); }))
        {
            return false;
        }

        data = std::move(queue_.front());
        queue_.pop();
        return true;
    }

    void PacketQueue::startProcessing(std::function<void(const std::vector<char> &)> processingFunction)
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

    void PacketQueue::processPackets(std::function<void(const std::vector<char> &)> processingFunction)
    {
        std::vector<char> data;
        while (running_)
        {
            if (tryDequeue(data))
            {
                processingFunction(data);
                data.clear(); // Reuse the vector
            }
        }
    }

} // namespace finance::infrastructure::network