// FinanceService.h
#pragma once

#include "../domain/Result.hpp"
#include "../domain/IFinanceRepository.h"
#include "../domain/IPackageHandler.h"
#include "../infrastructure/network/TcpServiceAdapter.h"
#include <memory>
#include <atomic>

namespace finance::application
{
    using finance::domain::ErrorResult;
    using finance::domain::Result;
    using finance::domain::SummaryData;

    class FinanceService
    {
    public:
        FinanceService(
            std::shared_ptr<finance::domain::IFinanceRepository<SummaryData, ErrorResult>> repository,
            std::shared_ptr<finance::domain::IPackageHandler> packetHandler);

        Result<void, ErrorResult> initialize();
        Result<void, ErrorResult> run();
        void shutdown();

    private:
        std::shared_ptr<finance::domain::IFinanceRepository<SummaryData, ErrorResult>> repository_;
        std::shared_ptr<finance::domain::IPackageHandler> packetHandler_;
        std::unique_ptr<infrastructure::network::TcpServiceAdapter> tcpService_;

        bool isInitialized_{false};
        bool isRunning_{false};
        std::atomic<int> signalStatus_{0};
    };
}
