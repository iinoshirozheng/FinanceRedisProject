#pragma once

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <thread>  // std::this_thread::yield
#include <cstring> // memchr
#include <stdexcept>
#include <cinttypes> // PRIu64
#include <loguru.hpp>
#include <optional>

namespace finance::infrastructure::network
{

    inline void cpu_pause() noexcept
    {
#if defined(__x86_64__) || defined(_M_X64)
        // x86/x64 上的 PAUSE hint
        __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__) || defined(__arm__)
        // ARM 上的 YIELD hint (AArch64 & ARMv7+)
        __asm__ __volatile__("yield" ::: "memory");
#else
        // 其他平台 fallback
        std::this_thread::yield();
#endif
    }

    /**
     * 單生產者單消費者（SPSC）環形緩衝區，編譯期固定容量 CAP（2 的冪次）。
     * 完全無鎖（0lock）實現，使用自旋等待取代 mutex/condition_variable。
     * 支援零拷貝多段讀寫、診斷日誌、版本號保護。
     * C++17 相容。
     */
    template <size_t CAP>
    class RingBuffer
    {
        static_assert(CAP > 0, "CAP 必須大於 0");
        static_assert((CAP & (CAP - 1)) == 0, "CAP 必須是 2 的冪次");

    public:
        struct PacketRef
        {
            size_t offset;
            size_t length;
        };

        // 代表一個封包的兩段記憶體
        struct PacketSeg
        {
            const char *ptr1;
            size_t len1;
            const char *ptr2;
            size_t len2;
            size_t totalLen() const noexcept { return len1 + len2; }
        };

        RingBuffer() noexcept
            : head_(0), tail_(0), clearGen_(0)
        {
            LOG_F(INFO, "RingBuffer<%zu> constructed (0lock)", CAP);
        }

        RingBuffer(const RingBuffer &) = delete;
        RingBuffer &operator=(const RingBuffer &) = delete;
        RingBuffer(RingBuffer &&) = default;
        RingBuffer &operator=(RingBuffer &&) = default;

        /// 取得版本號
        uint64_t generation() const noexcept
        {
            return clearGen_.load(std::memory_order_acquire);
        }

        /// 環形緩衝區總容量
        static constexpr size_t capacity() noexcept { return CAP; }

        /// 是否為空
        [[nodiscard]]
        bool empty() const noexcept
        {
            // 把 tail_ 也用 acquire，避免讀到過度陳舊的值
            return head_.load(std::memory_order_relaxed) == tail_.load(std::memory_order_acquire);
        }

        /// 當前可讀字節數
        [[nodiscard]]
        size_t size() const noexcept
        {
            size_t h = head_.load(std::memory_order_relaxed);
            size_t t = tail_.load(std::memory_order_acquire);
            return (t + CAP - h) & Mask;
        }

        /// 取得可寫指標與最大可寫長度（不跨環界）
        char *writablePtr(size_t &maxLen) noexcept
        {
            // 1) 先讀取消費者釋放的 head（需要 acquire）
            size_t h = head_.load(std::memory_order_acquire);
            // 2) 再讀取自己的 tail（只自己寫自己讀，可 relaxed）
            size_t t = tail_.load(std::memory_order_relaxed);

            // 3) 計算總可用空間
            size_t totalFree = (h + CAP - t - 1) & Mask;
            // 4) 計算第一段連續可寫長度，不跨界
            size_t idx = t & Mask;
            size_t first = CAP - idx;
            maxLen = totalFree < first ? totalFree : first;

            return buffer_ + idx;
        }

        /**
         * 將 n 字節資料入隊，使用者需先透過 writablePtr() 複製資料後呼叫。
         * @throws std::overflow_error 可寫空間不足
         */
        void enqueue(size_t n)
        {
            size_t avail;
            writablePtr(avail);
            if (n > avail)
            {
                LOG_F(ERROR,
                      "enqueue overflow: need=%zu avail=%zu head=%zu tail=%zu gen=%" PRIu64,
                      n, avail,
                      head_.load(std::memory_order_relaxed),
                      tail_.load(std::memory_order_relaxed),
                      generation());
                throw std::overflow_error("RingBuffer overflow on enqueue");
            }
            // 發佈寫入
            tail_.store(tail_.load(std::memory_order_relaxed) + n,
                        std::memory_order_release);
        }

        /// peek 第一段連續可讀資料（不跨環界）
        std::pair<const char *, size_t> peekFirst(size_t &len) const noexcept
        {
            size_t h = head_.load(std::memory_order_relaxed);
            size_t t = tail_.load(std::memory_order_acquire);
            size_t total = (t + CAP - h) & Mask;
            size_t idx = h & Mask;
            len = std::min(total, CAP - idx);
            return {buffer_ + idx, len};
        }

        /// peek 第二段資料（跨環界後部分）
        std::pair<const char *, size_t> peekSecond(size_t firstLen) const noexcept
        {
            size_t h = head_.load(std::memory_order_relaxed);
            size_t t = tail_.load(std::memory_order_acquire);
            size_t total = (t + CAP - h) & Mask;
            size_t idx = h & Mask;
            size_t len1 = std::min(total, CAP - idx);
            size_t wrap = (firstLen > len1 ? total : (total - len1));
            return {buffer_, wrap};
        }

        /**
         * 搜尋首個 '\n' 並填充 PacketRef，並回傳是否跨界
         */
        bool findPacket(PacketRef &ref, bool &isCrossBoundary) const noexcept
        {
            size_t h = head_.load(std::memory_order_acquire);
            size_t t = tail_.load(std::memory_order_acquire);
            size_t total = (t + CAP - h) & Mask;
            if (total == 0)
                return false;

            size_t idx = h & Mask;
            size_t len1 = std::min(total, CAP - idx);
            // 在第一段尋找 '\n'
            if (auto p = static_cast<const char *>(
                    std::memchr(buffer_ + idx, '\n', len1)))
            {
                ref.offset = h;
                ref.length = (p - (buffer_ + idx)) + 1;
                isCrossBoundary = false;
                return true;
            }
            // 在第二段尋找 '\n'
            size_t wrap = total - len1;
            if (auto q = static_cast<const char *>(
                    std::memchr(buffer_, '\n', wrap)))
            {
                ref.offset = h;
                ref.length = len1 + (q - buffer_) + 1;
                isCrossBoundary = true;
                return true;
            }
            return false;
        }

        /**
         * 搜尋首個 '\n' 並填充 PacketRef
         */
        bool findPacket(PacketRef &ref) const noexcept
        {
            bool dummy;
            return findPacket(ref, dummy);
        }

        /**
         * 將 n 字節資料出隊；失敗拋出 underflow_error。
         */
        void dequeue(size_t n)
        {
            size_t h = head_.load(std::memory_order_relaxed);
            size_t t = tail_.load(std::memory_order_acquire);
            size_t avail = (t + CAP - h) & Mask;
            if (n > avail)
            {
                LOG_F(ERROR,
                      "dequeue underflow: need=%zu avail=%zu head=%zu tail=%zu gen=%" PRIu64,
                      n, avail, h, t, generation());
                throw std::underflow_error("RingBuffer underflow on dequeue");
            }
            // 發佈消費
            head_.store((h + n) & Mask, std::memory_order_release);
        }

        /**
         * 取得任意 offset 的資料指標（自動做環界運算）
         */
        char *getDataPtr(size_t &offset) noexcept
        {
            return buffer_ + (offset & Mask);
        }

        /// 清空緩衝區並增加版本號（不超過 UINT64_MAX）
        void clear() noexcept
        {
            size_t t = tail_.load(std::memory_order_relaxed) & Mask;
            head_.store(t, std::memory_order_release);

            uint64_t old = clearGen_.load(std::memory_order_relaxed);
            uint64_t next = old + 1;
            if (next == 0) // Overflow occurred
            {
                LOG_F(WARNING, "RingBuffer<%zu> version number overflow, wrapping around", CAP);
            }
            clearGen_.store(next, std::memory_order_release);
        }

        /**
         * 自旋等待直到有資料
         */
        void waitForData() const noexcept
        {
            while (empty())
            {
                cpu_pause();
            }
        }

        /**
         * 自旋等待直到可寫空間 >= n
         */
        void waitForSpace(size_t n) const noexcept
        {
            while (true)
            {
                size_t h = head_.load(std::memory_order_acquire);
                size_t t = tail_.load(std::memory_order_relaxed);
                if (((h + CAP - t - 1) & Mask) >= n)
                    break;
                cpu_pause();
            }
        }

        /**
         * 讀取完整封包的兩段連續記憶體區塊
         * @return 若找到完整封包則回傳兩段區塊，否則回傳 nullopt
         */
        std::optional<PacketSeg> getNextPacket() const noexcept
        {
            // 1) 原子讀 head/tail
            size_t h = head_.load(std::memory_order_acquire);
            size_t t = tail_.load(std::memory_order_acquire);
            size_t total = (t + CAP - h) & Mask;
            if (total == 0)
                return std::nullopt;

            // 2) 計算第一段可讀起點與長度
            size_t idx = h & Mask;
            size_t len1 = std::min(total, CAP - idx);
            const char *p1 = buffer_ + idx;

            // 3) 在第一段找 '\n'
            if (auto p = static_cast<const char *>(std::memchr(p1, '\n', len1)))
            {
                size_t packetLen = (p - p1) + 1;
                return PacketSeg{p1, packetLen, nullptr, 0};
            }

            // 4) 在第二段找 '\n'
            size_t wrap = total - len1;
            if (auto q = static_cast<const char *>(std::memchr(buffer_, '\n', wrap)))
            {
                size_t packetLen = len1 + (q - buffer_) + 1;
                return PacketSeg{p1, len1, buffer_, packetLen - len1};
            }

            return std::nullopt;
        }

    private:
        static constexpr size_t Mask = CAP - 1;
        static constexpr size_t CACHELINE_SIZE = 64;

        alignas(CACHELINE_SIZE) char buffer_[CAP];
        alignas(CACHELINE_SIZE) std::atomic<size_t> head_;
        alignas(CACHELINE_SIZE) std::atomic<size_t> tail_;
        std::atomic<uint64_t> clearGen_;
    };

} // namespace finance::infrastructure::network
