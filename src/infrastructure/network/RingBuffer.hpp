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

        /// 環形緩衝區總容量 (實際可用容量是 CAP - 1)
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
            return (t - h + CAP) & Mask;
        }

        /// 取得可寫字節數
        [[nodiscard]]
        size_t free_space() const noexcept
        {
            size_t h = head_.load(std::memory_order_acquire); // 需要 acquire 確保讀到最新的 head_
            size_t t = tail_.load(std::memory_order_relaxed);
            // 可用空間是從 tail_ 到 head_ 之前的一個位置
            return (h - t + CAP - 1) & Mask;
        }

        /// 取得可寫指標與最大可寫長度（不跨環界）
        char *writablePtr(size_t &maxLen) noexcept
        {
            // 1) 先讀取消費者釋放的 head（需要 acquire）
            size_t h = head_.load(std::memory_order_acquire);
            // 2) 再讀取自己的 tail（只自己寫自己讀，可 relaxed）
            size_t t = tail_.load(std::memory_order_relaxed);

            // 3) 計算總可用空間 (注意這裡和 free_space() 計算方式稍有不同，以便計算第一段長度)
            size_t totalFree = (h + CAP - t - 1) & Mask;
            // 4) 計算第一段連續可寫長度，不跨界
            size_t idx = t & Mask;
            size_t first = CAP - idx;
            maxLen = totalFree < first ? totalFree : first;

            return buffer_ + idx;
        }

        /**
         * 將 n 字節資料入隊，使用者需先透過 writablePtr() 複製資料後呼叫。
         * 如果空間不足，將阻塞直到有足夠空間。
         * @throws std::logic_error 如果 n 大於 RingBuffer 的實際可用容量 (CAP - 1)
         */
        void enqueue(size_t n)
        {
            // 如果嘗試寫入的數據長度超過了 RingBuffer 的實際最大容量 (CAP-1)
            // 這是一個程式設計邏輯錯誤，因為單次寫入不可能超過這個大小。
            if (n == 0)
            {
                LOG_F(INFO, "Enqueueing 0 bytes.");
                return; // 寫入 0 字節直接返回成功
            }

            // 實際可用容量是 CAP - 1 (為區分滿和空)
            if (n > CAP - 1)
            {
                LOG_F(ERROR, "Enqueue size %zu exceeds actual RingBuffer capacity %zu - 1", n, CAP);
                throw std::logic_error("Enqueue size exceeds RingBuffer capacity");
            }

            // 自旋等待，直到有足夠的空間來寫入 n 個字節
            waitForSpace(n);

            // 確認在等待後， writablePtr() 返回的可用空間至少為 n。
            // 在 SPSC RingBuffer 中，waitForSpace(n) 確保了 (head_ - tail_ + CAP - 1) & Mask >= n。
            // writablePtr(avail) 返回的是從 tail_ 到末尾或 head_ 的第一個連續區塊長度。
            // 如果 RingBuffer 跨界，可能需要分兩次寫入。
            // 但是根據您 TcpServiceAdapter::producer 的邏輯，它是先 receiveBytes(writePtr, maxLen)
            // 之後再 enqueue(n)，其中 n <= maxLen。這表示 RingBuffer 的使用者已經處理了分塊寫入。
            // enqueue 方法只需要處理 tail_ 指針的更新和等待空間。

            // 發佈寫入：更新 tail 指針，通知消費者有新數據可用。
            // 使用 memory_order_release 確保在更新 tail_ 之前的所有寫入操作（數據複製到 buffer_）
            // 對消費者是可見的。
            tail_.store(tail_.load(std::memory_order_relaxed) + n,
                        std::memory_order_release);

            // RingBuffer 滿的邏輯已經改由 waitForSpace 處理，這裡不再需要檢查並拋出異常
            // 原始的 if (n > avail) 判斷塊被移除了。
        }

        /// peek 第一段連續可讀資料（不跨環界）
        std::pair<const char *, size_t> peekFirst(size_t &len) const noexcept
        {
            size_t h = head_.load(std::memory_order_relaxed);
            size_t t = tail_.load(std::memory_order_acquire); // 需要 acquire 確保讀到最新的 tail_
            size_t total = (t - h + CAP) & Mask;
            size_t idx = h & Mask;
            len = std::min(total, CAP - idx);
            return {buffer_ + idx, len};
        }

        /// peek 第二段資料（跨環界後部分）
        std::pair<const char *, size_t> peekSecond(size_t firstLen) const noexcept
        {
            size_t h = head_.load(std::memory_order_relaxed);
            size_t t = tail_.load(std::memory_order_acquire); // 需要 acquire 確保讀到最新的 tail_
            size_t total = (t - h + CAP) & Mask;
            size_t idx = h & Mask;
            size_t len1 = std::min(total, CAP - idx);
            // 如果 firstLen 大於第一段的實際長度，或者總長度就只有第一段，那麼第二段長度為 0
            size_t wrap = (firstLen < total) ? (total - len1) : 0;

            return {buffer_, wrap};
        }

        /**
         * 搜尋首個 '\n' 並填充 PacketRef，並回傳是否跨界
         */
        bool findPacket(PacketRef &ref, bool &isCrossBoundary) const noexcept
        {
            size_t h = head_.load(std::memory_order_acquire); // 需要 acquire 確保讀到最新的 head_ 和 tail_
            size_t t = tail_.load(std::memory_order_acquire);
            size_t total = (t - h + CAP) & Mask;
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
            // 在第二段尋找 '\n' (如果存在第二段的話)
            size_t wrap = total - len1;
            if (wrap > 0)
            {
                if (auto q = static_cast<const char *>(
                        std::memchr(buffer_, '\n', wrap)))
                {
                    ref.offset = h;
                    ref.length = len1 + (q - buffer_) + 1;
                    isCrossBoundary = true;
                    return true;
                }
            }
            return false; // 沒有找到 '\n'
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
         * 將 n 字節資料出隊；如果嘗試出隊的數量超過目前可用數據，則拋出 underflow_error。
         */
        void dequeue(size_t n)
        {
            size_t h = head_.load(std::memory_order_relaxed);
            size_t t = tail_.load(std::memory_order_acquire); // 需要 acquire 確保讀到最新的 tail_
            size_t avail = (t - h + CAP) & Mask;              // 當前可讀字節數

            if (n == 0)
            {
                // LOG_F(INFO, "Dequeuing 0 bytes.");
                return; // 出隊 0 字節直接返回
            }

            if (n > avail)
            {
                // 這通常是一個程式邏輯錯誤，消費者嘗試消費比可用數據更多的數據。
                // 拋出異常來指示這個錯誤可能是合理的，以便於調試。
                LOG_F(ERROR,
                      "dequeue underflow: need=%zu avail=%zu head=%zu tail=%zu gen=%" PRIu64,
                      n, avail, h, t, generation());
                throw std::underflow_error("RingBuffer underflow on dequeue");
            }
            // 發佈消費：更新 head 指針，通知生產者空間已釋放。
            // 使用 memory_order_release 確保在更新 head_ 之前的所有讀取操作（處理數據）
            // 對生產者是可見的。
            head_.store((h + n) & Mask, std::memory_order_release);
        }

        /**
         * 取得任意 offset 的資料指標（自動做環界運算）
         * @deprecated 這個方法可能不安全，應優先使用 peekFirst 和 peekSecond
         */
        // char *getDataPtr(size_t &offset) noexcept
        // {
        //     // 請注意：直接返回指標可能違反 SPSC 的原子性保證，
        //     // 在多個執行緒存取時可能不安全。
        //     // 建議改用 peekFirst/peekSecond 來獲取數據。
        //     return buffer_ + (offset & Mask);
        // }

        /// 清空緩衝區並增加版本號（不超過 UINT64_MAX）
        void clear() noexcept
        {
            // 清空操作需要同時重置 head_ 和 tail_
            // 使用 release 確保所有之前的寫入對其他執行緒可見，acquire 確保讀到最新的 tail_
            size_t t = tail_.load(std::memory_order_acquire);
            head_.store(t, std::memory_order_release);

            uint64_t old = clearGen_.load(std::memory_order_relaxed);
            uint64_t next = old + 1;
            if (next == 0) // Overflow occurred
            {
                LOG_F(WARNING, "RingBuffer<%zu> version number overflow, wrapping around", CAP);
            }
            clearGen_.store(next, std::memory_order_release);
            LOG_F(INFO, "RingBuffer cleared, new generation: %" PRIu64, generation());
        }

        /**
         * 自旋等待直到有資料
         */
        void waitForData() const noexcept
        {
            while (empty())
            {
                cpu_pause(); // 使用 cpu_pause 進行忙等
            }
            // LOG_F(DEBUG, "waitForData: data available.");
        }

        /**
         * 自旋等待直到可寫空間 >= n
         */
        void waitForSpace(size_t n) const noexcept
        {
            if (n == 0)
                return; // 等待 0 空間總是立即滿足

            while (true)
            {
                size_t h = head_.load(std::memory_order_acquire); // 需要 acquire 確保讀到最新的 head_
                size_t t = tail_.load(std::memory_order_relaxed);
                // 計算可用空間 (減 1 是為了區分滿和空)
                size_t free_space_val = (h - t + CAP - 1) & Mask;

                if (free_space_val >= n)
                {
                    // LOG_F(DEBUG, "waitForSpace: Space available (need %zu, free %zu)", n, free_space_val);
                    break; // 空間足夠，退出等待
                }

                // LOG_F(DEBUG, "waitForSpace: Waiting for space (need %zu, free %zu)", n, free_space_val);
                cpu_pause(); // 空間不足，進行短暫忙等
            }
        }

        /**
         * 讀取完整封包的兩段連續記憶體區塊
         * @return 若找到完整封包則回傳兩段區塊，否則回傳 nullopt
         */
        std::optional<PacketSeg> getNextPacket() const noexcept
        {
            // 1) 原子讀 head/tail
            // 使用 memory_order_acquire 確保讀到最新的 tail_ (生產者寫入的數據)
            size_t h = head_.load(std::memory_order_relaxed);
            size_t t = tail_.load(std::memory_order_acquire);
            size_t total = (t - h + CAP) & Mask; // 當前緩衝區中的數據總長度
            if (total == 0)
                return std::nullopt; // 緩衝區為空

            // 2) 計算第一段可讀起點與長度
            size_t idx = h & Mask;
            size_t len1 = std::min(total, CAP - idx); // 第一段連續可讀的長度
            const char *p1 = buffer_ + idx;

            // 3) 在第一段找 '\n'
            if (auto p = static_cast<const char *>(std::memchr(p1, '\n', len1)))
            {
                size_t packetLen = (p - p1) + 1; // 包含 '\n' 在內的封包總長度
                // 驗證封包長度是否合理 (例如大於某個最小長度)
                if (packetLen > total)
                {
                    // 邏輯錯誤：找到的封包長度超過了當前總數據長度
                    LOG_F(FATAL, "getNextPacket logic error: packetLen %zu > total %zu in first segment search.", packetLen, total);
                    // 在 SPSC 中這不應該發生，但在非 SPSC 或有其他問題時可能。
                    // 這裡返回 nullopt 或觸發錯誤處理。
                    return std::nullopt; // 或拋出異常
                }
                return PacketSeg{p1, packetLen, nullptr, 0}; // 封包在第一段
            }

            // 4) 如果第一段沒有找到 '\n'，並且有跨界數據，在第二段找 '\n'
            size_t wrap = total - len1; // 第二段的長度 (如果 RingBuffer 跨界)
            if (wrap > 0)
            {
                if (auto q = static_cast<const char *>(std::memchr(buffer_, '\n', wrap)))
                {
                    size_t packetLen = len1 + (q - buffer_) + 1; // 總長度 = 第一段長度 + 第二段中到 '\n' 的長度 + 1
                    // 驗證封包長度是否合理
                    if (packetLen > total)
                    {
                        LOG_F(FATAL, "getNextPacket logic error: packetLen %zu > total %zu in second segment search.", packetLen, total);
                        return std::nullopt;
                    }
                    return PacketSeg{p1, len1, buffer_, packetLen - len1}; // 封包跨越兩段
                }
            }

            // 如果緩衝區中有數據但沒有找到完整的封包 (以 '\n' 結尾)
            // LOG_F(DEBUG, "getNextPacket: Found data (total %zu) but no complete packet (no '\\n').", total);
            return std::nullopt; // 沒有找到完整的封包
        }

        // 提供公有方法來讀取 head_ 和 tail_ 的值（僅讀取）
        size_t getHead(std::memory_order order = std::memory_order_relaxed) const noexcept
        {
            return head_.load(order);
        }

        size_t getTail(std::memory_order order = std::memory_order_relaxed) const noexcept
        {
            return tail_.load(order);
        }

    private:
        static constexpr size_t Mask = CAP - 1;
        static constexpr size_t CACHELINE_SIZE = 64; // 通常為 64 字節

        // RingBuffer 緩衝區，使用 alignas 確保在快取行邊界對齊
        alignas(CACHELINE_SIZE) char buffer_[CAP];
        // 頭部指針，原子操作，表示下一個要讀取的數據位置 (消費者修改)
        alignas(CACHELINE_SIZE) std::atomic<size_t> head_;
        // 尾部指針，原子操作，表示下一個要寫入的數據位置 (生產者修改)
        alignas(CACHELINE_SIZE) std::atomic<size_t> tail_;
        // 清空操作版本號，用於診斷（可選）
        std::atomic<uint64_t> clearGen_;
    };

} // namespace finance::infrastructure::network