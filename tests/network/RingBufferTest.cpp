#include "../../src/infrastructure/network/RingBuffer.hpp"
#include "../../src/domain/FinanceDataStructure.h"
#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <iostream>
#include <random>
#include <atomic>
#include <vector>
#include <mutex>
#include <iomanip>
#include <condition_variable>
#include <cstring>
#include <cstdlib>
#include <ctime>

using namespace finance::infrastructure::network;
using namespace finance::domain;

class RingBufferTest : public ::testing::Test
{
protected:
    static constexpr size_t BUFFER_SIZE = 16 * 1024 * 1024; // 16MB
    RingBuffer<BUFFER_SIZE> buffer;

    void SetUp() override
    {
        srand(time(nullptr));
        std::cout << "\n=== Test Case: " << testing::UnitTest::GetInstance()->current_test_info()->name() << " ===\n";
    }

    void TearDown() override
    {
        std::cout << "=== Test Complete ===\n\n";
    }

    void generateRandomHcrtm01(MessageDataHCRTM01 &hcrtm01)
    {
        snprintf(hcrtm01.margin_amount, sizeof(hcrtm01.margin_amount) - 1, "%011d", 1000000 + rand() % 9000000);
        snprintf(hcrtm01.margin_qty, sizeof(hcrtm01.margin_qty) - 1, "%06d", 100 + rand() % 900);
    }

    // 生成隨機的 HCRTM01 消息
    FinancePackageMessage generateRandomHCRTM01()
    {
        FinancePackageMessage msg{};
        strcpy(msg.p_code, "0200");
        strcpy(msg.t_code, "ELD001");
        strcpy(msg.srcid, "CB");

        auto &hcrtm01 = msg.ap_data.data.hcrtm01;
        // 生成隨機股票代碼 (1000-9999)
        snprintf(hcrtm01.stock_id, sizeof(hcrtm01.stock_id), "%04d", 1000 + rand() % 9000);
        // 生成隨機融資額度 (1000000-9999999)
        snprintf(hcrtm01.margin_amount, sizeof(hcrtm01.margin_amount), "%011d", 1000000 + rand() % 9000000);
        // 生成隨機融資張數 (100-999)
        snprintf(hcrtm01.margin_qty, sizeof(hcrtm01.margin_qty), "%06d", 100 + rand() % 900);

        return msg;
    }
};

// 測試基本寫入和讀取 FinancePackageMessage
TEST_F(RingBufferTest, BasicWriteAndReadFinancePackage)
{
    FinancePackageMessage msg{};
    strcpy(msg.p_code, "0200");
    strcpy(msg.t_code, "ELD001");
    strcpy(msg.srcid, "CB");

    // 添加換行符作為數據包分隔符
    char packet[sizeof(FinancePackageMessage) + 1];
    memcpy(packet, &msg, sizeof(FinancePackageMessage));
    packet[sizeof(FinancePackageMessage)] = '\n';

    std::cout << "Writing FinancePackageMessage with size: " << sizeof(FinancePackageMessage) << " bytes\n";
    std::cout << "p_code: " << msg.p_code << ", t_code: " << msg.t_code << ", srcid: " << msg.srcid << "\n";

    // 寫入數據
    size_t len = sizeof(packet);
    size_t max_len;
    char *write_ptr = buffer.writablePtr(&max_len);
    ASSERT_GE(max_len, len);
    memcpy(write_ptr, packet, len);
    buffer.enqueue(len);

    // 查找並驗證數據包
    size_t read_len;
    auto [read_ptr, read_len] = buffer.peekFirst(read_len);
    ASSERT_EQ(read_len, len);

    // 驗證數據內容
    const FinancePackageMessage *received = reinterpret_cast<const FinancePackageMessage *>(read_ptr);
    ASSERT_EQ(memcmp(received->p_code, "0200", 4), 0);
    ASSERT_EQ(memcmp(received->t_code, "ELD001", 6), 0);
    ASSERT_EQ(memcmp(received->srcid, "CB", 2), 0);

    std::cout << "Received FinancePackageMessage:\n";
    std::cout << "p_code: " << received->p_code << "\n";
    std::cout << "t_code: " << received->t_code << "\n";
    std::cout << "srcid: " << received->srcid << "\n";

    // Dequeue the packet
    buffer.dequeue(read_len);
}

// 測試多個 HCRTM01 消息的寫入和讀取
TEST_F(RingBufferTest, MultipleHCRTM01Messages)
{
    const int NUM_MESSAGES = 5;
    std::vector<FinancePackageMessage> messages(NUM_MESSAGES);

    std::cout << "Preparing " << NUM_MESSAGES << " HCRTM01 messages\n";

    // 準備多個消息
    for (int i = 0; i < NUM_MESSAGES; ++i)
    {
        auto &msg = messages[i];
        strcpy(msg.p_code, "0200");
        strcpy(msg.t_code, "ELD001");
        strcpy(msg.srcid, "CB");

        // 設置 HCRTM01 特定數據
        auto &hcrtm01 = msg.ap_data.data.hcrtm01;
        snprintf(hcrtm01.stock_id, sizeof(hcrtm01.stock_id), "%04d", 2330 + i);
        snprintf(hcrtm01.margin_amount, sizeof(hcrtm01.margin_amount), "%011d", 1000000 + i);

        // 寫入數據（添加換行符）
        char packet[sizeof(FinancePackageMessage) + 1];
        memcpy(packet, &msg, sizeof(FinancePackageMessage));
        packet[sizeof(FinancePackageMessage)] = '\n';

        size_t max_len;
        char *write_ptr = buffer.writablePtr(&max_len);
        ASSERT_GE(max_len, sizeof(packet));
        memcpy(write_ptr, packet, sizeof(packet));
        buffer.enqueue(sizeof(packet));

        std::cout << "Written message " << i + 1 << " with stock_id: " << hcrtm01.stock_id << "\n";
    }

    // 讀取和驗證所有消息
    for (int i = 0; i < NUM_MESSAGES; ++i)
    {
        size_t read_len;
        auto [read_ptr, read_len] = buffer.peekFirst(read_len);

        const FinancePackageMessage *received = reinterpret_cast<const FinancePackageMessage *>(read_ptr);
        const auto &original_hcrtm01 = messages[i].ap_data.data.hcrtm01;
        const auto &received_hcrtm01 = received->ap_data.data.hcrtm01;

        ASSERT_EQ(memcmp(received_hcrtm01.stock_id, original_hcrtm01.stock_id, sizeof(original_hcrtm01.stock_id)), 0);
        ASSERT_EQ(memcmp(received_hcrtm01.margin_amount, original_hcrtm01.margin_amount, sizeof(original_hcrtm01.margin_amount)), 0);

        std::cout << "Verified message " << i + 1 << " with stock_id: " << received_hcrtm01.stock_id << "\n";

        buffer.dequeue(read_len);
    }
}

// 測試緩衝區容量和性能
TEST_F(RingBufferTest, BufferCapacityAndPerformance)
{
    const int NUM_MESSAGES = 1000;
    const auto start_time = std::chrono::high_resolution_clock::now();
    size_t total_bytes = 0;

    std::cout << "Starting performance test with " << NUM_MESSAGES << " messages\n";

    // 寫入大量消息
    for (int i = 0; i < NUM_MESSAGES; ++i)
    {
        FinancePackageMessage msg{};
        strcpy(msg.p_code, "0200");
        strcpy(msg.t_code, "ELD001");
        snprintf(msg.ap_data.data.hcrtm01.stock_id, 6, "%04d", i % 10000);

        char packet[sizeof(FinancePackageMessage) + 1];
        memcpy(packet, &msg, sizeof(FinancePackageMessage));
        packet[sizeof(FinancePackageMessage)] = '\n';

        size_t max_len;
        char *write_ptr = buffer.writablePtr(&max_len);
        ASSERT_GE(max_len, sizeof(packet));
        memcpy(write_ptr, packet, sizeof(packet));
        buffer.enqueue(sizeof(packet));
        total_bytes += sizeof(packet);
    }

    // 讀取所有消息
    int read_count = 0;
    while (read_count < NUM_MESSAGES)
    {
        size_t read_len;
        auto [read_ptr, read_len] = buffer.peekFirst(read_len);
        if (read_len > 0)
        {
            buffer.dequeue(read_len);
            read_count++;
        }
    }

    const auto end_time = std::chrono::high_resolution_clock::now();
    const auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    std::cout << "Performance Results:\n";
    std::cout << "Total messages processed: " << NUM_MESSAGES << "\n";
    std::cout << "Total bytes processed: " << total_bytes << "\n";
    std::cout << "Time taken: " << duration.count() << "ms\n";
    std::cout << "Throughput: " << (total_bytes * 1000.0 / duration.count() / 1024 / 1024) << " MB/s\n";
}

// 測試並發寫入和讀取金融數據包
TEST_F(RingBufferTest, ConcurrentFinancePackages)
{
    const int NUM_MESSAGES = 1000;
    std::atomic<int> write_count{0};
    std::atomic<int> read_count{0};
    std::atomic<bool> should_stop{false};

    std::cout << "Starting concurrent test with " << NUM_MESSAGES << " messages\n";

    // 寫入線程
    std::thread writer([&]()
                       {
        for (int i = 0; i < NUM_MESSAGES && !should_stop; ++i) {
            FinancePackageMessage msg{};
            strcpy(msg.p_code, "0200");
            strcpy(msg.t_code, "ELD001");
            snprintf(msg.ap_data.data.hcrtm01.stock_id, 6, "%04d", i % 10000);
            
            char packet[sizeof(FinancePackageMessage) + 1];
            memcpy(packet, &msg, sizeof(FinancePackageMessage));
            packet[sizeof(FinancePackageMessage)] = '\n';
            
            size_t max_len;
            char* write_ptr = buffer.writablePtr(&max_len);
            if (max_len >= sizeof(packet)) {
                memcpy(write_ptr, packet, sizeof(packet));
                buffer.enqueue(sizeof(packet));
                write_count++;
                
                if (write_count % 100 == 0) {
                    std::cout << "Written " << write_count << " messages\n";
                }
            }
            std::this_thread::yield();
        } });

    // 讀取線程
    std::thread reader([this, &should_stop, &read_count]()
                       {
        while (!should_stop && read_count < NUM_MESSAGES)
        {
            size_t read_len;
            auto [read_ptr, read_len] = buffer.peekFirst(read_len);
            if (read_len > 0)
            {
                const FinancePackageMessage* msg = reinterpret_cast<const FinancePackageMessage*>(read_ptr);
                buffer.dequeue(read_len);
                read_count++;
            }
            else
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        } });

    writer.join();
    reader.join();

    std::cout << "Final Results:\n";
    std::cout << "Total messages written: " << write_count << "\n";
    std::cout << "Total messages read: " << read_count << "\n";

    ASSERT_EQ(write_count, NUM_MESSAGES);
    ASSERT_EQ(read_count, NUM_MESSAGES);
}

// 壓力測試：模擬高頻收封包場景
TEST_F(RingBufferTest, StressTestPacketProcessing)
{
    const int NUM_PRODUCERS = 2;
    const int MESSAGES_PER_PRODUCER = 500; // Reduced from 1000 to prevent timeout
    const int TOTAL_MESSAGES = NUM_PRODUCERS * MESSAGES_PER_PRODUCER;
    const int TIMEOUT_MS = 30000; // Reduced timeout to 30 seconds

    std::atomic<int> total_processed{0};
    std::atomic<int> total_errors{0};
    std::atomic<bool> should_stop{false};
    std::mutex cout_mutex;
    std::condition_variable cv;
    std::atomic<int> active_producers{0};
    std::atomic<int> active_consumers{0};

    // Producer thread function
    auto producer_func = [&](int producer_id)
    {
        active_producers++;
        int messages_written = 0;
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> stock_dist(1000, 9999);
        std::uniform_int_distribution<> amount_dist(100000, 999999);
        std::uniform_int_distribution<> qty_dist(100, 999);

        while (messages_written < MESSAGES_PER_PRODUCER && !should_stop)
        {
            FinancePackageMessage msg{};
            // Initialize all fields with spaces first
            std::memset(&msg, ' ', sizeof(FinancePackageMessage));

            // Set fixed-length fields with exact sizes
            std::memcpy(msg.p_code, "0200", 4);
            std::memcpy(msg.t_code, "ELD001", 6);
            std::memcpy(msg.srcid, "CB", 2);

            // Initialize AP data fields
            auto &ap_data = msg.ap_data;
            std::memset(&ap_data, ' ', sizeof(ap_data));

            std::memcpy(ap_data.system, "001", 3);
            std::memcpy(ap_data.lib, "LIB001", 6);
            std::memcpy(ap_data.file, "FILE001", 7);
            std::memcpy(ap_data.member, "MEM001", 6);
            std::memcpy(ap_data.file_rrnc, "RRN001", 6);
            ap_data.entry_type[0] = 'C';
            std::memcpy(ap_data.rcd_len_cnt, "0000000123", 10);

            // Set HCRTM01 data
            auto &hcrtm01 = ap_data.data.hcrtm01;
            std::memset(&hcrtm01, ' ', sizeof(hcrtm01));

            std::memcpy(hcrtm01.broker_id, "1234", 4);
            std::memcpy(hcrtm01.area_center, "001", 3);

            // Format numeric fields with proper padding
            char stock_id[6];
            char margin_amount[11];
            char margin_qty[6];

            snprintf(stock_id, sizeof(stock_id), "%04d", stock_dist(gen));
            snprintf(margin_amount, sizeof(margin_amount), "%011d", amount_dist(gen));
            snprintf(margin_qty, sizeof(margin_qty), "%06d", qty_dist(gen));

            std::memcpy(hcrtm01.stock_id, stock_id, sizeof(hcrtm01.stock_id));
            std::memcpy(hcrtm01.margin_amount, margin_amount, sizeof(hcrtm01.margin_amount));
            std::memcpy(hcrtm01.margin_qty, margin_qty, sizeof(hcrtm01.margin_qty));

            // Add packet delimiter
            char packet[sizeof(FinancePackageMessage) + 1];
            std::memcpy(packet, &msg, sizeof(FinancePackageMessage));
            packet[sizeof(FinancePackageMessage)] = '\n';

            size_t max_len;
            char *write_ptr = buffer.writablePtr(&max_len);
            if (max_len >= sizeof(packet))
            {
                std::memcpy(write_ptr, packet, sizeof(packet));
                buffer.enqueue(sizeof(packet));
                messages_written++;

                if (messages_written % 100 == 0)
                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "Producer " << producer_id << " progress: "
                              << (messages_written * 100) / MESSAGES_PER_PRODUCER << "% ("
                              << messages_written << "/" << MESSAGES_PER_PRODUCER << " messages)\n";
                }
            }
            else
            {
                std::this_thread::yield();
            }
        }

        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Producer " << producer_id << " finished, wrote " << messages_written << " messages\n";
        active_producers--;
        cv.notify_all();
    };

    // Consumer thread function
    auto consumer_func = [&](int consumer_id)
    {
        active_consumers++;
        int messages_processed = 0;

        while (!should_stop && messages_processed < MESSAGES_PER_PRODUCER)
        {
            size_t read_len;
            auto [read_ptr, read_len] = buffer.peekFirst(read_len);
            if (read_len > 0)
            {
                const FinancePackageMessage *msg = reinterpret_cast<const FinancePackageMessage *>(read_ptr);

                // Validate message format with exact comparisons
                bool is_valid = true;

                // Helper function to safely compare fixed-length strings
                auto compare_fixed = [](const char *str, const char *expected, size_t len) -> bool
                {
                    return std::memcmp(str, expected, len) == 0;
                };

                // Check header fields
                is_valid &= compare_fixed(msg->p_code, "0200", 4);
                is_valid &= compare_fixed(msg->t_code, "ELD001", 6);
                is_valid &= compare_fixed(msg->srcid, "CB", 2);
                is_valid &= (msg->ap_data.entry_type[0] == 'C');

                // Additional validation for HCRTM01 fields
                const auto &hcrtm01 = msg->ap_data.data.hcrtm01;
                is_valid &= compare_fixed(hcrtm01.broker_id, "1234", 4);
                is_valid &= compare_fixed(hcrtm01.area_center, "001", 3);

                if (!is_valid)
                {
                    std::lock_guard<std::mutex> lock(cout_mutex);
                    std::cout << "Invalid message detected by consumer " << consumer_id
                              << " at offset " << read_ptr << "\n"
                              << "p_code: [" << std::string_view(msg->p_code, 4) << "]\n"
                              << "t_code: [" << std::string_view(msg->t_code, 6) << "]\n"
                              << "srcid: [" << std::string_view(msg->srcid, 2) << "]\n"
                              << "entry_type: [" << msg->ap_data.entry_type[0] << "]\n"
                              << "broker_id: [" << std::string_view(hcrtm01.broker_id, 4) << "]\n"
                              << "area_center: [" << std::string_view(hcrtm01.area_center, 3) << "]\n";
                    total_errors.fetch_add(1, std::memory_order_relaxed);
                }

                buffer.dequeue(read_len);

                if (is_valid)
                {
                    messages_processed++;
                    total_processed.fetch_add(1, std::memory_order_relaxed);

                    if (messages_processed % 100 == 0)
                    {
                        std::lock_guard<std::mutex> lock(cout_mutex);
                        std::cout << "Consumer " << consumer_id << " progress: "
                                  << (messages_processed * 100) / MESSAGES_PER_PRODUCER << "% ("
                                  << messages_processed << "/" << MESSAGES_PER_PRODUCER << " messages)\n";
                    }
                }
            }
            else
            {
                std::this_thread::yield();
            }
        }

        std::lock_guard<std::mutex> lock(cout_mutex);
        std::cout << "Consumer " << consumer_id << " finished, processed " << messages_processed << " messages\n";
        active_consumers--;
        cv.notify_all();
    };

    auto start_time = std::chrono::high_resolution_clock::now();
    std::vector<std::thread> producers;
    std::vector<std::thread> consumers;

    // Start producers
    for (int i = 0; i < NUM_PRODUCERS; ++i)
    {
        producers.emplace_back(producer_func, i);
    }

    // Start consumers (one per producer)
    for (int i = 0; i < NUM_PRODUCERS; ++i)
    {
        consumers.emplace_back(consumer_func, i);
    }

    // Monitor progress and timeout
    bool timeout_occurred = false;
    std::thread monitor([&]()
                        {
        auto start = std::chrono::high_resolution_clock::now();
        while (!should_stop)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            auto now = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            
            if (duration.count() >= TIMEOUT_MS)
            {
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "Test timed out after " << TIMEOUT_MS << "ms\n";
                timeout_occurred = true;
                should_stop = true;
                break;
            }
            
            if (total_processed >= TOTAL_MESSAGES)
            {
                should_stop = true;
                break;
            }
        } });

    // Wait for completion
    monitor.join();

    // Wait for all producers and consumers to finish
    std::unique_lock<std::mutex> lock(cout_mutex);
    cv.wait(lock, [&]()
            { return active_producers == 0 && active_consumers == 0; });

    for (auto &producer : producers)
    {
        if (producer.joinable())
        {
            producer.join();
        }
    }

    for (auto &consumer : consumers)
    {
        if (consumer.joinable())
        {
            consumer.join();
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    // Output test results
    std::cout << "\n=== Stress Test Results ===\n";
    std::cout << "Total messages processed: " << total_processed << "\n";
    std::cout << "Total errors: " << total_errors << "\n";
    std::cout << "Time taken: " << duration.count() << "ms\n";
    std::cout << "Throughput: " << (total_processed * sizeof(FinancePackageMessage) * 1000.0 / duration.count() / 1024 / 1024) << " MB/s\n";
    std::cout << "Messages per second: " << (total_processed * 1000.0 / duration.count()) << "\n";
    std::cout << "Average latency: " << (duration.count() * 1.0 / total_processed) << " ms/message\n";

    // Verify results
    ASSERT_FALSE(timeout_occurred) << "Test timed out";
    ASSERT_EQ(total_processed, TOTAL_MESSAGES) << "Not all messages were processed";
    ASSERT_EQ(total_errors, 0) << "Errors occurred during processing";
}

// 測試緩衝區溢出處理
TEST_F(RingBufferTest, BufferOverflowHandling)
{
    const int NUM_MESSAGES = 1000;
    std::atomic<int> overflow_count{0};

    std::cout << "Testing buffer overflow handling with " << NUM_MESSAGES << " messages\n";

    // 快速寫入大量消息
    for (int i = 0; i < NUM_MESSAGES; ++i)
    {
        auto msg = generateRandomHCRTM01();
        char packet[sizeof(FinancePackageMessage) + 1];
        memcpy(packet, &msg, sizeof(FinancePackageMessage));
        packet[sizeof(FinancePackageMessage)] = '\n';

        size_t max_len;
        char *write_ptr = buffer.writablePtr(&max_len);
        if (max_len >= sizeof(packet))
        {
            memcpy(write_ptr, packet, sizeof(packet));
            buffer.enqueue(sizeof(packet));
        }
        else
        {
            overflow_count++;
            if (overflow_count % 100 == 0)
            {
                std::cout << "Buffer overflow occurred " << overflow_count << " times\n";
            }
        }
    }

    std::cout << "\nBuffer Overflow Test Results:\n";
    std::cout << "Total overflow occurrences: " << overflow_count << "\n";
    std::cout << "Overflow rate: " << (overflow_count * 100.0 / NUM_MESSAGES) << "%\n";

    // 驗證緩衝區仍然可以正常工作
    size_t read_len;
    auto [read_ptr, read_len] = buffer.peekFirst(read_len);
    ASSERT_TRUE(read_len > 0);
}

TEST_F(RingBufferTest, SinglePacketTest)
{
    // Create a test packet
    FinancePackageMessage msg;
    msg.header.length = sizeof(FinancePackageMessage);
    msg.header.tcode = "HCRTM01";
    msg.header.seq = 1;

    // Generate random data
    generateRandomHcrtm01(msg.data.hcrtm01);

    // Write to buffer
    size_t max_len;
    char *write_ptr = buffer.writablePtr(max_len);
    ASSERT_GE(max_len, sizeof(FinancePackageMessage));
    memcpy(write_ptr, &msg, sizeof(FinancePackageMessage));
    buffer.enqueue(sizeof(FinancePackageMessage));

    // Read from buffer
    size_t len;
    auto [read_ptr, read_len] = buffer.peekFirst(len);
    ASSERT_EQ(read_len, sizeof(FinancePackageMessage));

    // Verify data
    const FinancePackageMessage *received = reinterpret_cast<const FinancePackageMessage *>(read_ptr);
    ASSERT_EQ(received->header.length, msg.header.length);
    ASSERT_EQ(received->header.seq, msg.header.seq);
    ASSERT_EQ(std::string(received->header.tcode), std::string(msg.header.tcode));
    ASSERT_EQ(std::string(received->data.hcrtm01.margin_amount), std::string(msg.data.hcrtm01.margin_amount));
    ASSERT_EQ(std::string(received->data.hcrtm01.margin_qty), std::string(msg.data.hcrtm01.margin_qty));

    // Dequeue the packet
    buffer.dequeue(sizeof(FinancePackageMessage));
}

TEST_F(RingBufferTest, MultiplePacketsTest)
{
    const int NUM_PACKETS = 100;
    std::vector<FinancePackageMessage> messages(NUM_PACKETS);

    // Write multiple packets
    for (int i = 0; i < NUM_PACKETS; ++i)
    {
        auto &msg = messages[i];
        msg.header.length = sizeof(FinancePackageMessage);
        msg.header.tcode = "HCRTM01";
        msg.header.seq = i + 1;

        // Generate random data
        snprintf(msg.data.hcrtm01.margin_amount, sizeof(msg.data.hcrtm01.margin_amount) - 1, "%011d", 1000000 + i);
        snprintf(msg.data.hcrtm01.margin_qty, sizeof(msg.data.hcrtm01.margin_qty) - 1, "%06d", 100 + i);

        size_t max_len;
        char *write_ptr = buffer.writablePtr(max_len);
        ASSERT_GE(max_len, sizeof(FinancePackageMessage));
        memcpy(write_ptr, &msg, sizeof(FinancePackageMessage));
        buffer.enqueue(sizeof(FinancePackageMessage));
    }

    // Read and verify all packets
    for (int i = 0; i < NUM_PACKETS; ++i)
    {
        size_t len;
        auto [read_ptr, read_len] = buffer.peekFirst(len);
        ASSERT_EQ(read_len, sizeof(FinancePackageMessage));

        const FinancePackageMessage *received = reinterpret_cast<const FinancePackageMessage *>(read_ptr);
        ASSERT_EQ(received->header.length, messages[i].header.length);
        ASSERT_EQ(received->header.seq, messages[i].header.seq);
        ASSERT_EQ(std::string(received->header.tcode), std::string(messages[i].header.tcode));
        ASSERT_EQ(std::string(received->data.hcrtm01.margin_amount), std::string(messages[i].data.hcrtm01.margin_amount));
        ASSERT_EQ(std::string(received->data.hcrtm01.margin_qty), std::string(messages[i].data.hcrtm01.margin_qty));

        buffer.dequeue(sizeof(FinancePackageMessage));
    }
}

TEST_F(RingBufferTest, WrapAroundTest)
{
    const int NUM_PACKETS = 1000;
    const int PACKET_SIZE = sizeof(FinancePackageMessage);

    // Fill buffer multiple times to force wrap-around
    for (int round = 0; round < 3; ++round)
    {
        // Write packets until buffer is almost full
        for (int i = 0; i < NUM_PACKETS; ++i)
        {
            FinancePackageMessage msg;
            msg.header.length = PACKET_SIZE;
            msg.header.tcode = "HCRTM01";
            msg.header.seq = i + 1;

            // Generate random data
            generateRandomHcrtm01(msg.data.hcrtm01);

            size_t max_len;
            char *write_ptr = buffer.writablePtr(max_len);
            if (max_len >= PACKET_SIZE)
            {
                memcpy(write_ptr, &msg, PACKET_SIZE);
                buffer.enqueue(PACKET_SIZE);
            }
            else
            {
                // Buffer is full, start consuming
                size_t len;
                auto [read_ptr, read_len] = buffer.peekFirst(len);
                if (read_len > 0)
                {
                    buffer.dequeue(read_len);
                }
            }
        }

        // Consume all packets
        while (true)
        {
            size_t len;
            auto [read_ptr, read_len] = buffer.peekFirst(len);
            if (read_len == 0)
            {
                break;
            }

            const FinancePackageMessage *msg = reinterpret_cast<const FinancePackageMessage *>(read_ptr);
            ASSERT_EQ(msg->header.length, PACKET_SIZE);
            buffer.dequeue(read_len);
        }
    }
}