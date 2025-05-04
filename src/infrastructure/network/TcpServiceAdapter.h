#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <Poco/Net/ServerSocket.h>
#include "RingBuffer.hpp"
#include "../../domain/IPackageHandler.h"
#include "../config/ConnectionConfigProvider.hpp"

namespace finance::infrastructure::network
{
    static constexpr int SOCKET_TIMEOUT_MS = 1000;               // ms
    static constexpr size_t RING_BUFFER_SIZE = 16 * 1024 * 1024; // 16MB

    class TcpServiceAdapter
    {
    public:
        explicit TcpServiceAdapter(std::shared_ptr<domain::IPackageHandler> handler)
            : serverSocket_{{"0.0.0.0", static_cast<unsigned short>(config::ConnectionConfigProvider::serverPort())}},
              handler_(std::move(handler)),
              ringBuffer_()
        {
            serverSocket_.setReuseAddress(true);
            serverSocket_.setReusePort(true);
        }

        ~TcpServiceAdapter();
        bool start();
        void stop();

    private:
        void Producer();
        void Consumer();

        Poco::Net::ServerSocket serverSocket_;
        std::shared_ptr<domain::IPackageHandler> handler_;
        RingBuffer<RING_BUFFER_SIZE> ringBuffer_;
        std::thread acceptThread_;
        std::thread processingThread_;
        std::atomic<bool> running_{false};
    };
}
