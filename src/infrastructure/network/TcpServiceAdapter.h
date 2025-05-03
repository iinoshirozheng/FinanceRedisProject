#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <Poco/Net/ServerSocket.h>
#include "RingBuffer.hpp"
#include "../../domain/IPackageHandler.h"

namespace finance::infrastructure::network
{
    static constexpr int SOCKET_TIMEOUT_MS = 1000;               // ms
    static constexpr size_t RING_BUFFER_SIZE = 16 * 1024 * 1024; // 16MB

    class TcpServiceAdapter
    {
    public:
        TcpServiceAdapter(int port,
                          std::shared_ptr<domain::IPackageHandler> handler);
        ~TcpServiceAdapter();

        bool start();
        void stop();

    private:
        void Producer();
        void Consumer();

        Poco::Net::ServerSocket serverSocket_;
        std::shared_ptr<domain::IPackageHandler> handler_;
        std::atomic<bool> running_{false};

        // 改成直接成員，效能更好
        RingBuffer<RING_BUFFER_SIZE> ringBuffer_;

        std::thread acceptThread_;
        std::thread processingThread_;
    };
}
