#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <memory>

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
                std::queue<char *> queue_;
                std::mutex queueMutex_;
                std::condition_variable queueCondition_;
                std::atomic<bool> running_;
                std::thread processingThread_;

                /**
                 * 數據包處理線程函數
                 * @param processingFunction 處理每個數據包的函數
                 */
                void processPackets(std::function<void(char *)> processingFunction);
            };

        } // namespace network
    } // namespace infrastructure
} // namespace finance