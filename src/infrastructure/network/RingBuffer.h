#pragma once

#include <atomic>
#include <cstddef>
#include <cstring>
#include <memory>
#include <mutex>
#include <condition_variable>

namespace finance::infrastructure::network
{
    struct PacketRef
    {
        size_t offset;
        size_t length;
    };

    class RingBuffer
    {
    public:
        explicit RingBuffer(size_t capacity)
            : buf_(new char[capacity]), capacity_(capacity),
              head_(0), tail_(0) {}

        ~RingBuffer() = default;

        // Get pointer to writable area and its length
        char *writablePtr(size_t *maxLen)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t free_space = freeSpace();
            *maxLen = std::min(free_space, capacity_ - tail_);
            return buf_.get() + tail_;
        }

        // Advance write pointer after direct write
        void advanceWrite(size_t len)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            tail_ = (tail_ + len) % capacity_;
            condition_.notify_one();
        }

        // Consumer: Find next packet ending with '\n'
        bool findPacket(PacketRef &ref)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            size_t read = head_;
            size_t available = size();

            for (size_t i = 0; i < available; ++i)
            {
                if (buf_[(read + i) % capacity_] == '\n')
                {
                    ref.offset = read;
                    ref.length = i + 1; // Include '\n'
                    return true;
                }
            }
            return false;
        }

        // Consumer: Consume a packet
        void consume(size_t len)
        {
            std::lock_guard<std::mutex> lock(mutex_);
            head_ = (head_ + len) % capacity_;
        }

        // Get raw buffer pointer
        const char *data() const { return buf_.get(); }

        // Wait for data with timeout
        bool waitForData(std::chrono::milliseconds timeout)
        {
            std::unique_lock<std::mutex> lock(mutex_);
            return condition_.wait_for(lock, timeout, [this]
                                       { return size() > 0; });
        }

    private:
        size_t size() const
        {
            return (tail_ + capacity_ - head_) % capacity_;
        }

        size_t freeSpace() const
        {
            return capacity_ - size() - 1; // Leave one byte empty to distinguish full from empty
        }

        std::unique_ptr<char[]> buf_;
        const size_t capacity_;
        std::atomic<size_t> head_{0};
        std::atomic<size_t> tail_{0};
        std::mutex mutex_;
        std::condition_variable condition_;
    };

} // namespace finance::infrastructure::network