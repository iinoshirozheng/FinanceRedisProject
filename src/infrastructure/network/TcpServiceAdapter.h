#pragma once

#include <memory>
#include <thread>
#include <atomic>
#include <Poco/Net/ServerSocket.h>
#include "RingBuffer.hpp"
#include "../config/ConnectionConfigProvider.hpp"
#include "../../domain/IPackageHandler.h"
#include "../../domain/IFinanceRepository.h"
#include "../../domain/FinanceDataStructure.h"

namespace finance::infrastructure::network
{
    static constexpr size_t RING_BUFFER_SIZE = 16 * 1024 * 1024; // 16MB

    class TcpServiceAdapter
    {
    public:
        explicit TcpServiceAdapter(std::shared_ptr<finance::domain::IPackageHandler> handler,
                                   std::shared_ptr<finance::domain::IFinanceRepository<finance::domain::SummaryData, finance::domain::ErrorResult>> repository)
            : serverSocket_{{"0.0.0.0", static_cast<unsigned short>(config::ConnectionConfigProvider::serverPort())}},
              handler_(std::move(handler)),
              repository_(std::move(repository)),
              ringBuffer_()
        {
            serverSocket_.setReuseAddress(true);
            serverSocket_.setReusePort(true);
            serverSocket_.setReceiveTimeout(Poco::Timespan(config::ConnectionConfigProvider::socketTimeoutMs() * 1000));
        }

        ~TcpServiceAdapter();
        bool start();
        void stop();

    private:
        void producer();
        void consumer();

        Poco::Net::ServerSocket serverSocket_;
        std::shared_ptr<finance::domain::IPackageHandler> handler_;
        std::shared_ptr<finance::domain::IFinanceRepository<finance::domain::SummaryData, finance::domain::ErrorResult>> repository_;
        RingBuffer<RING_BUFFER_SIZE> ringBuffer_;
        std::thread acceptThread_;
        std::thread processingThread_;
        std::atomic<bool> running_{false};
        std::mutex cvMutex_;
        std::condition_variable cv_;
    };
}
