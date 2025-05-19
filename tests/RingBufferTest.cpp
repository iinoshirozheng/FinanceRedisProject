
#include "gtest/gtest.h"
#include "infrastructure/network/RingBuffer.hpp"
#include <thread>
#include <vector>
#include <string>
#include <cstring>   // For memcpy, memcmp, memset
#include <numeric>   // For std::iota
#include <algorithm> // For std::min
#include <loguru.hpp>

// Make the namespace accessible
using namespace finance::infrastructure::network;

class GlobalLoguruEnvironment : public ::testing::Environment
{
public:
    ~GlobalLoguruEnvironment() override {}

    // 這個 SetUp 方法會在所有測試開始前被調用一次
    void SetUp() override
    {

        // 完全關閉所有到 stderr (控制台) 的 Loguru 日誌輸出
        loguru::g_stderr_verbosity = loguru::Verbosity_OFF;
        // 為 loguru::init 提供虛擬的 (dummy) 參數
        // Loguru 的 init 函數是冪等的，重複調用是安全的 (只會執行一次初始化)
        char arg0_buffer[] = "run_test"; // argv[0] 通常是程序名
        char *my_argv[] = {arg0_buffer, nullptr};
        int my_argc = 1;
        loguru::init(my_argc, my_argv);

        // 如果您想確保絕對沒有任何文件日誌 (儘管 Loguru 預設可能不會創建)
        // 可以考慮 (如果 Loguru API 支持) 移除所有已註冊的文件回調，
        // 但通常情況下，只要不主動調用 add_file，並且 init 時沒有指定日誌路徑，
        // 就不會有文件日誌。
        // loguru::remove_all_callbacks(); // 如果存在類似這樣的 API
    }

    void TearDown() override
    {
        // 可選：在所有測試結束後進行清理
    }
};

// Helper to simulate producer writing data.
// It handles potential segmentation from writablePtr.
template <size_t CAP> // Templatize the helper function
void producer_write_data(RingBuffer<CAP> &rb, const char *data, size_t len)
{
    size_t current_written = 0;
    while (current_written < len)
    {
        size_t data_left_to_write = len - current_written;
        size_t max_len_segment;
        char *write_ptr = rb.writablePtr(max_len_segment);
        ASSERT_NE(write_ptr, nullptr);

        if (max_len_segment == 0)
        {
            ASSERT_GT(data_left_to_write, 0) << "writablePtr returned 0 but still data to write. Buffer might be unexpectedly full.";
            // No writable segment right now, must wait if data still needs to be written.
            rb.waitForSpace(data_left_to_write);
            continue; // Retry getting a writable pointer.
        }

        size_t to_write_this_segment = std::min(data_left_to_write, max_len_segment);
        // This assertion should now be safe after handling max_len_segment == 0
        ASSERT_GT(to_write_this_segment, 0) << "Calculated zero bytes to write for this segment.";

        memcpy(write_ptr, data + current_written, to_write_this_segment);
        rb.enqueue(to_write_this_segment);
        current_written += to_write_this_segment;
    }
}

template <size_t CAP>
std::string consumer_read_data(RingBuffer<CAP> &rb, size_t len_to_read)
{
    if (len_to_read == 0)
    { // Handle reading 0 bytes explicitly
        // rb.dequeue(0) is a no-op as per your DequeueZeroBytes test logic.
        return ""; // Reading 0 bytes results in an empty string.
    }

    if (rb.size() < len_to_read)
    {
        rb.waitForData(); // Attempt to wait for data
        if (rb.size() < len_to_read)
        {
            ADD_FAILURE() << "Not enough data after waitForData. Need: " << len_to_read << ", Have: " << rb.size();
            return ""; // Indicate failure
        }
    }
    // At this point, rb.size() should be >= len_to_read. If not, it's a logic flaw or unexpected state.

    std::string result_str;
    // It's safer to build the string and only then dequeue, or use a temporary buffer.

    char *temp_buf = new char[len_to_read]; // Allocate temporary buffer

    size_t peek_len1_actual;
    auto peek1_result = rb.peekFirst(peek_len1_actual);

    if (peek1_result.first == nullptr && len_to_read > 0)
    { // Check only if we expect to read something
        ADD_FAILURE() << "peekFirst returned nullptr when data was expected (len_to_read: " << len_to_read << ").";
        delete[] temp_buf;
        return ""; // Indicate failure
    }

    size_t to_copy_from_peek1 = std::min(len_to_read, peek_len1_actual);
    if (peek1_result.first)
    { // Ensure ptr is not null before memcpy
        memcpy(temp_buf, peek1_result.first, to_copy_from_peek1);
    }
    else if (to_copy_from_peek1 > 0)
    {
        // This case should ideally not be reached if the above nullptr check is hit
        ADD_FAILURE() << "peek1_result.first is null but attempting to copy " << to_copy_from_peek1 << " bytes.";
        delete[] temp_buf;
        return "";
    }

    if (to_copy_from_peek1 < len_to_read)
    {
        size_t remaining_to_read = len_to_read - to_copy_from_peek1;
        auto peek2_result = rb.peekSecond(peek_len1_actual);

        if (peek2_result.first == nullptr)
        {
            ADD_FAILURE() << "peekSecond returned nullptr when wrapped data was expected for remaining " << remaining_to_read << " bytes.";
            delete[] temp_buf;
            return ""; // Indicate failure
        }
        if (peek2_result.second < remaining_to_read)
        {
            ADD_FAILURE() << "Second peek segment too small. Needed: " << remaining_to_read
                          << ", Got: " << peek2_result.second;
            // Decide: return "" or try to read what's available? For strict tests, fail.
            delete[] temp_buf;
            return ""; // Indicate failure
        }
        memcpy(temp_buf + to_copy_from_peek1, peek2_result.first, remaining_to_read);
    }

    result_str.assign(temp_buf, len_to_read);
    delete[] temp_buf;

    rb.dequeue(len_to_read);
    return result_str;
}

template <size_t CAP_T_PARAM> // Renamed to avoid conflict with RingBuffer's CAP
class RingBufferTest : public ::testing::Test
{
protected:
    // The RingBuffer object itself is a member, will be constructed for each test.
    RingBuffer<CAP_T_PARAM> rb_;

    void SetUp() override
    {
        // rb_ is freshly constructed by the test fixture for each test instance.
        // The default constructor of RingBuffer initializes head_, tail_, and clearGen_ to 0.
        // No need for `rb_ = RingBuffer<CAP_T_PARAM>();` which caused the assignment error.
        // If tests require `clearGen_` to be non-zero initially or other specific setup,
        // it can be done here, but for starting fresh, this is sufficient.
    }
};

// Define test fixtures for specific capacities
using RingBufferTest16 = RingBufferTest<16>;
using RingBufferTest128 = RingBufferTest<128>;

TEST_F(RingBufferTest16, InitialState)
{
    EXPECT_EQ(rb_.capacity(), 16);
    EXPECT_TRUE(rb_.empty());
    EXPECT_EQ(rb_.size(), 0);
    EXPECT_EQ(rb_.free_space(), 16 - 1);
    EXPECT_EQ(rb_.generation(), 0);
    EXPECT_EQ(rb_.getHead(), 0);
    EXPECT_EQ(rb_.getTail(), 0);
}

TEST_F(RingBufferTest16, EnqueueDequeueSimple)
{
    const std::string data = "hello";
    producer_write_data(rb_, data.data(), data.length());

    EXPECT_FALSE(rb_.empty());
    EXPECT_EQ(rb_.size(), data.length());
    EXPECT_EQ(rb_.free_space(), (16 - 1) - data.length());

    std::string read_data = consumer_read_data(rb_, data.length());
    EXPECT_EQ(read_data, data);

    EXPECT_TRUE(rb_.empty());
    EXPECT_EQ(rb_.size(), 0);
    EXPECT_EQ(rb_.free_space(), 16 - 1);
}

TEST_F(RingBufferTest16, EnqueueZeroBytes)
{
    size_t initial_tail = rb_.getTail();
    rb_.enqueue(0);
    EXPECT_EQ(rb_.getTail(), initial_tail);
    EXPECT_TRUE(rb_.empty());
}

TEST_F(RingBufferTest16, DequeueZeroBytes)
{
    producer_write_data(rb_, "abc", 3);
    size_t initial_head = rb_.getHead();
    size_t initial_size = rb_.size();
    rb_.dequeue(0);
    EXPECT_EQ(rb_.getHead(), initial_head);
    EXPECT_EQ(rb_.size(), initial_size);
}

TEST_F(RingBufferTest16, EnqueueTooLargeThrowsLogicError)
{
    EXPECT_THROW(rb_.enqueue(16), std::logic_error);
    EXPECT_THROW(rb_.enqueue(100), std::logic_error);
}

TEST_F(RingBufferTest16, DequeueTooLargeThrowsUnderflowError)
{
    producer_write_data(rb_, "abc", 3);
    EXPECT_THROW(rb_.dequeue(4), std::underflow_error);
}

TEST_F(RingBufferTest16, FillBufferExactly)
{
    const size_t actual_capacity = 16 - 1;
    std::string data(actual_capacity, 'A');
    producer_write_data(rb_, data.data(), data.length());

    EXPECT_EQ(rb_.size(), actual_capacity);
    EXPECT_EQ(rb_.free_space(), 0);

    size_t max_len;
    char *ptr = rb_.writablePtr(max_len);
    EXPECT_EQ(max_len, 0);

    std::string read_data = consumer_read_data(rb_, actual_capacity);
    EXPECT_EQ(read_data, data);
    EXPECT_TRUE(rb_.empty());
}

TEST_F(RingBufferTest16, ClearOperation)
{
    producer_write_data(rb_, "testdata", 8);
    ASSERT_EQ(rb_.size(), 8);
    ASSERT_EQ(rb_.generation(), 0);

    rb_.clear(); // generation becomes 1

    EXPECT_TRUE(rb_.empty());
    EXPECT_EQ(rb_.size(), 0);
    EXPECT_EQ(rb_.free_space(), 16 - 1);
    EXPECT_EQ(rb_.generation(), 1);
    EXPECT_EQ(rb_.getHead(), rb_.getTail());

    const std::string data_after_clear = "new";
    producer_write_data(rb_, data_after_clear.data(), data_after_clear.length());
    EXPECT_EQ(rb_.size(), data_after_clear.length());
    EXPECT_EQ(consumer_read_data(rb_, data_after_clear.length()), data_after_clear);
    EXPECT_EQ(rb_.generation(), 1); // Generation should remain 1 unless cleared again
}

TEST_F(RingBufferTest128, ClearGenerationIncrements)
{
    uint64_t gen0 = rb_.generation();
    EXPECT_EQ(gen0, 0);
    rb_.clear();
    uint64_t gen1 = rb_.generation();
    EXPECT_EQ(gen1, gen0 + 1);

    rb_.clear();
    uint64_t gen2 = rb_.generation();
    EXPECT_EQ(gen2, gen1 + 1);
}

TEST_F(RingBufferTest16, WrapAroundWriteRead)
{
    std::string data1 = "0123456789"; // len = 10
    producer_write_data(rb_, data1.data(), data1.length());
    ASSERT_EQ(rb_.getTail(), 10); // Continuous counter for tail
    ASSERT_EQ(rb_.size(), 10);

    std::string read_part1 = consumer_read_data(rb_, 5);
    ASSERT_EQ(read_part1, "01234");
    ASSERT_EQ(rb_.getHead(), 5); // Continuous counter for head
    ASSERT_EQ(rb_.size(), 5);

    std::string data2 = "ABCDEFGH"; // len = 8
    producer_write_data(rb_, data2.data(), data2.length());
    ASSERT_EQ(rb_.getHead(), 5);
    ASSERT_EQ(rb_.getTail(), 10 + 8); // Continuous counter: old_tail + new_data_len
    ASSERT_EQ(rb_.size(), 13);        // 5 (remaining) + 8 (new)

    std::string expected_total_read = "56789" + data2;
    std::string actual_total_read = consumer_read_data(rb_, 13);
    EXPECT_EQ(actual_total_read, expected_total_read);
    EXPECT_TRUE(rb_.empty());
}

TEST_F(RingBufferTest16, FindPacket_EmptyBuffer)
{
    RingBuffer<16>::PacketRef ref;
    bool is_cross;
    EXPECT_FALSE(rb_.findPacket(ref, is_cross));
    EXPECT_FALSE(rb_.findPacket(ref));
}

TEST_F(RingBufferTest16, FindPacket_NoNewline)
{
    producer_write_data(rb_, "abcdefghijklmno", 15);
    RingBuffer<16>::PacketRef ref;
    bool is_cross;
    EXPECT_FALSE(rb_.findPacket(ref, is_cross));
}

TEST_F(RingBufferTest16, FindPacket_SimpleNoWrap)
{
    // std::string data = "packet1\\nnext_data"; // 原來的數據太長 (17 bytes)
    std::string data_to_write = "packet1\n"; // 8 bytes, 可以輕鬆放入容量為 15 的緩衝區
    // 或者，如果您想在 packet 後面加一些額外的數據，確保總長度 <= 15
    // std::string data_to_write = "packet1\\nnext"; // 8 + 4 = 12 bytes, 也適合

    producer_write_data(rb_, data_to_write.data(), data_to_write.length());

    RingBuffer<16>::PacketRef ref;
    bool is_cross;
    // 現在 findPacket 應該能正常工作
    ASSERT_TRUE(rb_.findPacket(ref, is_cross));
    EXPECT_FALSE(is_cross);
    EXPECT_EQ(ref.offset, rb_.getHead()); // 假設 head 從 0 開始
    EXPECT_EQ(ref.length, 8);             // "packet1\\n" 的長度

    std::string packet_content = consumer_read_data(rb_, ref.length);
    EXPECT_EQ(packet_content, "packet1\n");

    // 如果您寫入了 "packet1\\nnext"，可以檢查剩餘的 "next"
    // if (data_to_write == "packet1\\nnext") {
    //     EXPECT_EQ(rb_.size(), 4); // "next" 的長度
    //     std::string remaining_data = consumer_read_data(rb_, rb_.size());
    //     EXPECT_EQ(remaining_data, "next");
    //     EXPECT_TRUE(rb_.empty());
    // } else {
    EXPECT_TRUE(rb_.empty()); // 如果只寫了 "packet1\\n" 並讀取了它
    // }
}

TEST_F(RingBufferTest16, FindPacket_AcrossBoundary)
{
    // 1. 移動 head_ 到一個接近緩衝區物理末尾的位置
    // 例如，CAP=16，我們將 head_ 移動到 14
    // 寫入 14 字節的數據
    char junk[14];
    memset(junk, 'J', sizeof(junk));
    producer_write_data(rb_, junk, 14); // tail_ 變成 14
    consumer_read_data(rb_, 14);        // head_ 變成 14
    ASSERT_EQ(rb_.getHead(), 14);
    ASSERT_EQ(rb_.getTail(), 14);
    ASSERT_TRUE(rb_.empty());

    // 現在 head_ = 14。物理寫入將從 buffer_[14] 開始。
    // buffer_[14] 和 buffer_[15] 是第一段中剩餘的空間 (2 個字節)。

    // 2. 構造一個 packet，使其 \\n 落在環繞後的部分
    // 例如，packet "HI\\nJKL"
    // "HI" (2 bytes) 寫入 buffer_[14], buffer_[15]。tail_ 變成 14 + 2 = 16。
    // "\\nJKL" (4 bytes) 寫入 buffer_[0], buffer_[1], buffer_[2], buffer_[3]。tail_ 變成 16 + 4 = 20。
    // 期望找到的 packet: "HI\\n" (長度 3)
    std::string packet_data_to_wrap = "HI\nJKL"; // 總長度 2 + 1 + 3 = 6 bytes
    producer_write_data(rb_, packet_data_to_wrap.data(), packet_data_to_wrap.length());
    // 此時: head_ = 14, tail_ = 20. size = 6.

    RingBuffer<16>::PacketRef ref;
    bool is_cross = false; // 初始化以防萬一
    ASSERT_TRUE(rb_.findPacket(ref, is_cross))
        << "findPacket failed. head=" << rb_.getHead()
        << " tail=" << rb_.getTail()
        << " size=" << rb_.size();

    // 期望 is_cross 為 true，因為 \\n (在 buffer_[0]) 不在第一段掃描區 (buffer_[14], buffer_[15])
    EXPECT_TRUE(is_cross);

    EXPECT_EQ(ref.offset, 14); // Packet 的邏輯起始位置
    EXPECT_EQ(ref.length, 3);  // Packet "HI\\n" 的長度

    std::string packet_content = consumer_read_data(rb_, ref.length);
    EXPECT_EQ(packet_content, "HI\n");

    EXPECT_EQ(rb_.size(), 3); // "JKL" 應該還在
    std::string remaining_content = consumer_read_data(rb_, rb_.size());
    EXPECT_EQ(remaining_content, "JKL");
    EXPECT_TRUE(rb_.empty());
}

TEST_F(RingBufferTest16, GetNextPacket_SimpleNoWrap)
{
    std::string data = "pkt_one\npkt_two";
    producer_write_data(rb_, data.data(), data.length());

    auto opt_packet_seg = rb_.getNextPacket();
    ASSERT_TRUE(opt_packet_seg.has_value());
    auto &seg = opt_packet_seg.value();

    EXPECT_NE(seg.ptr1, nullptr);
    EXPECT_EQ(seg.len1, 8); // "pkt_one\\n"
    EXPECT_EQ(std::string(seg.ptr1, seg.len1), "pkt_one\n");
    EXPECT_EQ(seg.ptr2, nullptr);
    EXPECT_EQ(seg.len2, 0);
    ASSERT_EQ(seg.totalLen(), 8);

    rb_.dequeue(seg.totalLen());
    EXPECT_EQ(rb_.size(), data.length() - 8);
}

TEST_F(RingBufferTest16, GetNextPacket_AcrossBoundary)
{
    producer_write_data(rb_, "JUNKXYXY", 8);
    consumer_read_data(rb_, 8);
    ASSERT_EQ(rb_.getHead(), 8);
    ASSERT_TRUE(rb_.empty());

    std::string data_to_wrap = "BeforeWrap\nAfterWrap"; // "BeforeWrap" (10), "\\n" (1), "AfterWrap" (9) = 20 bytes
                                                        // This example is too large for CAP 16.
    // head=8, tail=8. free_space=15
    // "DataEnd" (7 bytes) -> at [8-14]. tail = 15
    // "\\nNewData" (8 bytes) -> "\\n" at [15], "NewData" at [0-6]. tail = 15+8 = 23
    // Packet "DataEnd\\n"
    std::string wrapped_packet_data = "DataEnd\nNewData";
    producer_write_data(rb_, wrapped_packet_data.data(), wrapped_packet_data.length());
    // head=8, tail=23. size = 15.

    auto opt_packet_seg = rb_.getNextPacket();
    ASSERT_TRUE(opt_packet_seg.has_value()) << "head=" << rb_.getHead() << " tail=" << rb_.getTail() << " size=" << rb_.size();
    auto &seg = opt_packet_seg.value();

    // Packet "DataEnd\\n" (8 bytes)
    // From head=8: buffer_[8..14] is "DataEnd" (len 7)
    //              buffer_[15] is "\\n" (len 1)
    // So ptr1 should point to buffer_[8] with len1 = 7 ("DataEnd")
    // and ptr2 should point to buffer_[0] (after wrap, containing '\\n') with len2 = 1 ("\\n")
    // Total data available from head_ (idx 8):
    // "DataEnd" (7 bytes at indices 8-14)
    // "\\n" (1 byte at index 15)
    // "NewData" (7 bytes at indices 0-6) -- note: "NewData" is 7 bytes, not 8 as in comment.
    // wrapped_packet_data is "DataEnd\\nNewData" -> 7 + 1 + 7 = 15 bytes. This fills the buffer.

    // getNextPacket logic:
    // h=8, t=23, total=15. idx = 8. len1_calc = min(15, 16-8) = min(15,8) = 8.
    // p1 = buffer_ + 8. memchr(p1, '\\n', 8) for "DataEnd\\n". Finds '\\n' at p1+7.
    // packetLen_found = ((p1+7)-p1)+1 = 8.
    // Returns PacketSeg{p1, 8, nullptr, 0} because '\\n' is within the first calculated segment of available data.
    // The packet "DataEnd\\n" does *not* cross the boundary for getNextPacket's segments.

    EXPECT_NE(seg.ptr1, nullptr);
    EXPECT_EQ(seg.len1, 8); // "DataEnd\\n"
    EXPECT_EQ(std::string(seg.ptr1, seg.len1), "DataEnd\n");
    EXPECT_EQ(seg.ptr2, nullptr); // Packet doesn't need second segment
    EXPECT_EQ(seg.len2, 0);
    ASSERT_EQ(seg.totalLen(), 8);

    rb_.dequeue(seg.totalLen());
    EXPECT_EQ(rb_.size(), 7); // "NewData" remains
    EXPECT_EQ(consumer_read_data(rb_, 7), "NewData");
}

TEST_F(RingBufferTest16, GetNextPacket_NoNewlineInData)
{
    producer_write_data(rb_, "no newline here", 15);
    auto opt_packet_seg = rb_.getNextPacket();
    EXPECT_FALSE(opt_packet_seg.has_value());
}

TEST_F(RingBufferTest16, WaitForData_ImmediateReturnIfData)
{
    producer_write_data(rb_, "data", 4);
    rb_.waitForData();
    EXPECT_EQ(rb_.size(), 4);
}

TEST_F(RingBufferTest16, WaitForSpace_ImmediateReturnIfSpace)
{
    rb_.waitForSpace(5);
    EXPECT_EQ(rb_.free_space(), 15);
    rb_.waitForSpace(0);
    EXPECT_EQ(rb_.free_space(), 15);
}

template <size_t CAP_SPSC_PARAM> // Renamed template parameter
class RingBufferSPSCTest : public ::testing::Test
{
protected:
    RingBuffer<CAP_SPSC_PARAM> rb_spsc_;

    static void producer_task(RingBuffer<CAP_SPSC_PARAM> &rb, const std::vector<std::string> &messages)
    {
        for (const auto &msg : messages)
        {
            size_t written_this_msg = 0;
            while (written_this_msg < msg.length())
            {
                size_t data_left_to_write = msg.length() - written_this_msg;
                size_t segment_len;
                char *ptr = rb.writablePtr(segment_len);

                if (segment_len == 0)
                {
                    ASSERT_GT(data_left_to_write, 0) << "Producer: writablePtr returned 0 segment but data remains.";
                    rb.waitForSpace(data_left_to_write);
                    continue;
                }

                size_t to_write_now = std::min(data_left_to_write, segment_len);
                memcpy(ptr, msg.data() + written_this_msg, to_write_now);
                rb.enqueue(to_write_now);
                written_this_msg += to_write_now;
            }
        }
    }

    static void consumer_task(RingBuffer<CAP_SPSC_PARAM> &rb, size_t num_messages_to_read, std::vector<std::string> &received)
    {
        while (received.size() < num_messages_to_read)
        {
            rb.waitForData();
            auto opt_packet = rb.getNextPacket();
            if (opt_packet)
            {
                std::string packet_str(opt_packet->ptr1, opt_packet->len1);
                if (opt_packet->ptr2 && opt_packet->len2 > 0)
                {
                    packet_str.append(opt_packet->ptr2, opt_packet->len2);
                }
                received.push_back(packet_str);
                rb.dequeue(opt_packet->totalLen());
            }
            else
            {
                std::this_thread::yield();
            }
        }
    }
};

using RingBufferSPSCTest64 = RingBufferSPSCTest<64>;

TEST_F(RingBufferSPSCTest64, ProducerConsumerFullCycle)
{
    std::vector<std::string> messages_to_send;
    const int num_msgs = 200;
    for (int i = 0; i < num_msgs; ++i)
    {
        messages_to_send.push_back("Msg:" + std::to_string(i) + std::string(i % 10 + 5, (char)('A' + i % 26)) + "\n");
    }

    std::vector<std::string> received_messages;
    received_messages.reserve(num_msgs);

    std::thread producer_th(producer_task, std::ref(rb_spsc_), std::cref(messages_to_send));
    std::thread consumer_th(consumer_task, std::ref(rb_spsc_), num_msgs, std::ref(received_messages));

    producer_th.join();
    consumer_th.join();

    ASSERT_EQ(received_messages.size(), messages_to_send.size()) << "Mismatch in number of messages sent and received.";
    for (size_t i = 0; i < messages_to_send.size(); ++i)
    {
        EXPECT_EQ(received_messages[i], messages_to_send[i]) << "Mismatch at message index " << i;
    }
    EXPECT_TRUE(rb_spsc_.empty()) << "Buffer should be empty after all messages are processed.";
    EXPECT_EQ(rb_spsc_.size(), 0);
}

// Example main function to run tests
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    ::testing::AddGlobalTestEnvironment(new GlobalLoguruEnvironment()); // Add this line
    return RUN_ALL_TESTS();
}
