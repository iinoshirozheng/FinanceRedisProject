#pragma once

#include <Poco/Net/TCPServer.h>
#include <Poco/Net/TCPServerConnection.h>
#include <Poco/Net/TCPServerConnectionFactory.h>
#include <Poco/Net/StreamSocket.h>
#include <Poco/Buffer.h>

#include "../../domain/IPacketHandler.h"
#include "PacketQueue.h"
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>

namespace finance
{
    namespace infrastructure
    {
        namespace network
        {

            /**
             * 用於緩衝和分發傳入數據包數據的類
             */
            class PacketDispatcher
            {
            public:
                /**
                 * 構造函數
                 * @param maxBufferSize 最大緩衝區大小
                 * @param packetQueue 用於分發數據包的隊列
                 */
                PacketDispatcher(size_t maxBufferSize, std::shared_ptr<PacketQueue> packetQueue);

                /**
                 * 析構函數
                 */
                ~PacketDispatcher();

                /**
                 * 向緩衝區添加數據
                 * @param data 要添加的數據
                 * @param size 數據的大小
                 */
                void addData(const char *data, size_t size);

                /**
                 * 啟動數據包分發線程
                 */
                void startDispatching();

                /**
                 * 停止數據包分發線程
                 */
                void stopDispatching();

            private:
                Poco::Buffer<char> buffer_;
                std::mutex bufferMutex_;
                std::shared_ptr<PacketQueue> packetQueue_;
                std::atomic<bool> running_;
                std::thread dispatchThread_;

                /**
                 * 數據包分發線程函數
                 */
                void dispatchPackets();
            };

            /**
             * TCP 服務連接實現
             */
            class FinanceServiceConnection : public Poco::Net::TCPServerConnection
            {
            public:
                /**
                 * 構造函數
                 * @param socket 連接的套接字
                 * @param dispatcher 要使用的數據包分發器
                 */
                FinanceServiceConnection(const Poco::Net::StreamSocket &socket,
                                         std::shared_ptr<PacketDispatcher> dispatcher);

                /**
                 * 運行連接
                 */
                void run() override;

            private:
                std::shared_ptr<PacketDispatcher> dispatcher_;
            };

            /**
             * TCP 服務連接工廠實現
             */
            class FinanceServiceConnectionFactory : public Poco::Net::TCPServerConnectionFactory
            {
            public:
                /**
                 * 構造函數
                 * @param dispatcher 要使用的數據包分發器
                 */
                explicit FinanceServiceConnectionFactory(std::shared_ptr<PacketDispatcher> dispatcher);

                /**
                 * 創建新連接
                 * @param socket 連接的套接字
                 * @return 新的連接實例
                 */
                Poco::Net::TCPServerConnection *createConnection(
                    const Poco::Net::StreamSocket &socket) override;

            private:
                std::shared_ptr<PacketDispatcher> dispatcher_;
            };

            /**
             * 主要 TCP 服務適配器
             */
            class TcpServiceAdapter
            {
            public:
                /**
                 * 構造函數
                 * @param port 要監聽的端口
                 * @param packetHandler 金融票據的處理器
                 */
                TcpServiceAdapter(int port, std::shared_ptr<domain::IPackageHandler> packetHandler);

                /**
                 * 析構函數
                 */
                ~TcpServiceAdapter();

                /**
                 * 啟動服務
                 * @return 如果成功啟動則返回真
                 */
                bool start();

                /**
                 * 停止服務
                 */
                void stop();

            private:
                int port_;
                std::shared_ptr<domain::IPackageHandler> packetHandler_;
                std::unique_ptr<Poco::Net::TCPServer> server_;
                std::shared_ptr<PacketQueue> packetQueue_;
                std::shared_ptr<PacketDispatcher> packetDispatcher_;

                /**
                 * 處理金融票據數據包
                 * @param data 數據包數據
                 */
                void processPacket(char *data);
            };

        } // namespace network
    } // namespace infrastructure
} // namespace finance