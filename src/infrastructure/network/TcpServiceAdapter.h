#pragma once

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/StreamSocket.h>
#include <memory>
#include <thread>
#include <atomic>
#include "PacketQueue.h"
#include "../../domain/FinanceDataStructure.h"
#include "../../domain/IPacketHandler.h"

namespace finance::infrastructure::network
{
    // Constants for TCP service
    static constexpr int MAX_BUFFER_SIZE = 4096;   // Maximum size of a single receive buffer
    static constexpr int MAX_PACKET_SIZE = 4000;   // Maximum size of a single packet
    static constexpr int SOCKET_TIMEOUT_MS = 1000; // Socket timeout in milliseconds
    static constexpr int RECEIVE_RETRY_MS = 10;    // Time to wait when queue is full

    /**
     * 主要 TCP 服務適配器
     * 負責：
     * 1. 接受 TCP 連接
     * 2. 接收和解析數據包
     * 3. 將數據包加入處理隊列
     */
    class TcpServiceAdapter
    {
    public:
        /**
         * 構造函數
         * @param port 要監聽的端口
         * @param packetHandler 金融票據的處理器
         */
        explicit TcpServiceAdapter(int port, std::shared_ptr<domain::IPackageHandler> handler);

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
        void acceptLoop();
        void handleClient(Poco::Net::StreamSocket socket);
        bool receiveData(Poco::Net::StreamSocket &socket, char *buffer, int length);
        bool parseMessage(std::string_view buffer, domain::FinancePackageMessage &message);

        Poco::Net::ServerSocket serverSocket_;
        std::shared_ptr<domain::IPackageHandler> handler_;
        std::atomic<bool> running_{false};
        std::thread acceptThread_;
        PacketQueue packetQueue_;
    };

} // namespace finance::infrastructure::network