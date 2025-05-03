#pragma once

#include <cstddef>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <loguru.hpp>
#include <vector>

namespace finance::infrastructure::network
{
    constexpr size_t MEGA_BYTE = 1024 * 1024;
    constexpr size_t MIN_BUFFER_CAPACITY = 8 * MEGA_BYTE;
    constexpr size_t MAX_BUFFER_CAPACITY = 128 * MEGA_BYTE;

    class RingBuffer
    {
    public:
        struct PacketRef
        {
            size_t offset;
            size_t length;
        };

        explicit RingBuffer(size_t minCapacity = 1024, size_t maxCapacity = 1024 * 1024 * 1024)
            : minCapacity_(minCapacity), maxCapacity_(maxCapacity)
        {
            if (minCapacity > maxCapacity)
            {
                throw std::invalid_argument("minCapacity cannot be greater than maxCapacity");
            }
            buffer_.resize(minCapacity);
        }

        ~RingBuffer()
        {
            // No need to delete buffer_ as it's managed by std::vector
        }

        // Prevent copying
        RingBuffer(const RingBuffer &) = delete;
        RingBuffer &operator=(const RingBuffer &) = delete;

        // Allow moving
        RingBuffer(RingBuffer &&other) noexcept
            : buffer_(other.buffer_), readPos_(other.readPos_), writePos_(other.writePos_),
              minCapacity_(other.minCapacity_), maxCapacity_(other.maxCapacity_)
        {
            other.buffer_.clear();
            other.readPos_ = 0;
            other.writePos_ = 0;
            other.minCapacity_ = 0;
            other.maxCapacity_ = 0;
        }

        RingBuffer &operator=(RingBuffer &&other) noexcept
        {
            if (this != &other)
            {
                buffer_.clear();
                minCapacity_ = other.minCapacity_;
                maxCapacity_ = other.maxCapacity_;
                readPos_ = other.readPos_;
                writePos_ = other.writePos_;
                other.minCapacity_ = 0;
                other.maxCapacity_ = 0;
                other.readPos_ = 0;
                other.writePos_ = 0;
            }
            return *this;
        }

        char *writablePtr(size_t *max_len)
        {
            if (max_len == nullptr)
            {
                return nullptr;
            }

            size_t available = writableSize();
            if (available == 0)
            {
                *max_len = 0;
                return nullptr;
            }

            *max_len = available;
            return buffer_.data() + writePos_;
        }

        void advanceWrite(size_t len)
        {
            if (len > writableSize())
            {
                throw std::runtime_error("Cannot advance write position beyond buffer capacity");
            }
            writePos_ = (writePos_ + len) % buffer_.size();
        }

        bool findPacket(PacketRef &ref)
        {
            size_t available = readableSize();
            if (available == 0)
            {
                return false;
            }

            // Search for packet delimiter
            size_t search_len = std::min(available, buffer_.size() - readPos_);
            char *search_start = buffer_.data() + readPos_;
            char *delimiter = static_cast<char *>(std::memchr(search_start, '\n', search_len));

            if (delimiter == nullptr && search_len < available)
            {
                // Check the remaining part of the buffer
                search_len = available - search_len;
                delimiter = static_cast<char *>(std::memchr(buffer_.data(), '\n', search_len));
            }

            if (delimiter == nullptr)
            {
                return false;
            }

            ref.offset = readPos_;
            ref.length = (delimiter - search_start) + 1; // Include the delimiter
            return true;
        }

        void consume(size_t len)
        {
            if (len > readableSize())
            {
                throw std::runtime_error("Cannot consume more data than available");
            }
            readPos_ = (readPos_ + len) % buffer_.size();
        }

        size_t size() const
        {
            if (writePos_ >= readPos_)
            {
                return writePos_ - readPos_;
            }
            return buffer_.size() - (readPos_ - writePos_);
        }

        size_t capacity() const
        {
            return buffer_.size();
        }

        bool empty() const
        {
            return readPos_ == writePos_;
        }

        bool full() const
        {
            return size() == buffer_.size() - 1;
        }

        void clear()
        {
            readPos_ = 0;
            writePos_ = 0;
        }

        const char *data() const
        {
            return buffer_.data();
        }

        inline bool resizeBuffer(size_t newCapacity)
        {
            if (newCapacity > MAX_BUFFER_CAPACITY)
            {
                LOG_F(ERROR, "Attempted to resize buffer beyond maximum capacity (%zu MB)", static_cast<size_t>(MAX_BUFFER_CAPACITY / MEGA_BYTE));
                return false;
            }

            if (newCapacity <= buffer_.size())
            {
                LOG_F(WARNING, "Resize skipped: new capacity (%zu) is not larger than current capacity (%zu)", newCapacity, buffer_.size());
                return false;
            }

            buffer_.resize(newCapacity);
            readPos_ = 0;
            writePos_ = size();

            LOG_F(INFO, "Buffer resized to capacity: %zu MB", newCapacity / MEGA_BYTE);
            return true;
        }

        inline bool shrinkBuffer(size_t newCapacity)
        {
            if (newCapacity < MIN_BUFFER_CAPACITY)
            {
                LOG_F(ERROR, "Attempted to shrink buffer below minimum capacity (%zu MB)", static_cast<size_t>(MIN_BUFFER_CAPACITY / MEGA_BYTE));
                return false;
            }

            if (newCapacity < size())
            {
                LOG_F(ERROR, "Shrink failed: new capacity (%zu) cannot accommodate current data (%zu)", newCapacity, size());
                return false;
            }

            if (newCapacity >= buffer_.size())
            {
                LOG_F(WARNING, "Shrink skipped: new capacity (%zu) is not smaller than current capacity (%zu)", newCapacity, buffer_.size());
                return false;
            }

            buffer_.resize(newCapacity);
            readPos_ = 0;
            writePos_ = size();

            LOG_F(INFO, "Buffer shrunk to capacity: %zu MB", newCapacity / MEGA_BYTE);
            return true;
        }

        inline void adjustBufferSize(int MaxRate = 5, int MinRate = 5)
        {
            size_t currentSize = size();
            size_t remainingSpace = writableSize();

            // Expand if space is low
            if (remainingSpace < currentSize / MaxRate)
            {
                size_t newCapacity = std::min(buffer_.size() * 2, static_cast<size_t>(MAX_BUFFER_CAPACITY));
                resizeBuffer(newCapacity);
                return;
            }

            // Shrink if utilization is low
            if (currentSize < buffer_.size() / MinRate && buffer_.size() > MIN_BUFFER_CAPACITY)
            {
                size_t newCapacity = std::max(buffer_.size() / 2, static_cast<size_t>(MIN_BUFFER_CAPACITY));
                shrinkBuffer(newCapacity);
                return;
            }
        }

    private:
        size_t readableSize() const
        {
            if (writePos_ >= readPos_)
            {
                return writePos_ - readPos_;
            }
            return buffer_.size() - (readPos_ - writePos_);
        }

        size_t writableSize() const
        {
            if (readPos_ > writePos_)
            {
                return readPos_ - writePos_ - 1;
            }
            return buffer_.size() - writePos_ - (readPos_ == 0 ? 0 : 1);
        }

        std::vector<char> buffer_;
        size_t readPos_{0};
        size_t writePos_{0};
        size_t minCapacity_;
        size_t maxCapacity_;
    };

} // namespace finance::infrastructure::network