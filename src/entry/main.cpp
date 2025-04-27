#include "domain/FinanceDataStructures.h"
#include "domain/IFinanceRepository.h"
#include "domain/IPacketHandler.h"
#include "application/FinanceService.h"
#include "infrastructure/storage/RedisSummaryAdapter.h"
#include "infrastructure/storage/AreaBranchAdapter.h"
#include "infrastructure/storage/ConfigAdapter.h"
#include "infrastructure/network/TcpServiceAdapter.h"
#include "../lib/loguru/loguru.hpp"

#include <memory>
#include <iostream>
#include <thread>
#include <csignal>

namespace
{
    // Signal handling flag
    volatile std::sig_atomic_t gSignalStatus = 0;
}

// Signal handler function
void signalHandler(int signal)
{
    gSignalStatus = signal;
}

int main(int argc, char **argv)
{
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("finance_system.log", loguru::Append, loguru::Verbosity_MAX);

    LOG_F(INFO, "Finance System starting up");

    try
    {
        // Check if we need to initialize indices
        bool initIndices = (argc > 1);

        // Load configuration
        auto configProvider = std::make_shared<finance::infrastructure::storage::ConfigAdapter>();
        if (!configProvider->loadFromFile("connection.json"))
        {
            LOG_F(WARNING, "Failed to load configuration, using defaults");
        }
        auto config = configProvider->getConfig();
        config.initializeIndices = initIndices;

        // Initialize area branch mapping
        auto areaBranchRepo = std::make_shared<finance::infrastructure::storage::AreaBranchAdapter>();
        if (!areaBranchRepo->loadFromFile("area_branch.json"))
        {
            LOG_F(ERROR, "Failed to load area-branch mapping");
            return 1;
        }

        // Initialize Redis repository
        LOG_F(INFO, "Connecting to Redis at %s", config.redisUrl.c_str());
        auto repository = std::make_shared<finance::infrastructure::storage::RedisSummaryAdapter>(
            config.redisUrl);

        if (config.initializeIndices)
        {
            LOG_F(INFO, "Initializing search indices");
            repository->createSearchIndex();
        }

        // Create finance service
        auto financeService = std::make_shared<finance::application::FinanceService>(
            repository, areaBranchRepo);

        // Load existing data
        LOG_F(INFO, "Loading existing summary data");
        financeService->loadAllSummaryData();

        // Create service factory
        auto serviceFactory = std::make_shared<finance::application::FinanceServiceFactory>(
            financeService);

        // Create bill handler
        auto billHandler = serviceFactory->createFinanceBillHandler();

        // Create and start TCP service
        LOG_F(INFO, "Starting TCP service on port %d", config.serverPort);
        finance::infrastructure::network::TcpServiceAdapter tcpService(
            config.serverPort,
            billHandler);

        tcpService.start();

        // Set up signal handling
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        // Main loop
        LOG_F(INFO, "Finance System running, press Ctrl+C to stop");
        while (gSignalStatus == 0)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Shut down
        LOG_F(INFO, "Shutting down Finance System");
        tcpService.stop();

        return 0;
    }
    catch (const std::exception &ex)
    {
        LOG_F(ERROR, "Unhandled exception: %s", ex.what());
        return 1;
    }
}