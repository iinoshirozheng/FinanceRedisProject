#pragma once

#include "../domain/FinanceDataStructure.h"
#include "../domain/IFinanceRepository.h"
#include "../domain/IPackageHandler.h"
#include "../domain/Status.h"
#include "../infrastructure/config/AreaBranchProvider.hpp"
#include "../infrastructure/config/ConnectionConfigProvider.hpp"
#include "../infrastructure/network/TcpServiceAdapter.h"
#include "../infrastructure/storage/RedisSummaryAdapter.h"
#include "../infrastructure/network/FinancePackageHandler.h"
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <optional>
#include <csignal>
#include <thread>
#include <atomic>

namespace finance::application
{
    class FinanceService
    {
    public:
        FinanceService() = default;

        // Initialize the service with configuration
        domain::Status initialize(const std::string &configPath);

        // Start the service main loop
        domain::Status run();

        // Stop the service gracefully
        void shutdown();

    private:
        infrastructure::network::PacketProcessorFactory packetHandler_;
        infrastructure::storage::RedisSummaryAdapter repository_;
        std::unique_ptr<infrastructure::network::TcpServiceAdapter> tcpService_;
        std::atomic<int> signalStatus_{0};
        bool isInitialized_{false};
        bool isRunning_{false};
    };
} // namespace finance::application