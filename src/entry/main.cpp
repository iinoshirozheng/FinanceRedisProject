#include "../application/FinanceService.h"
#include "../infrastructure/config/ConnectionConfigProvider.hpp"
#include "../infrastructure/config/AreaBranchProvider.hpp"
#include "../infrastructure/network/FinancePackageHandler.h"
#include "../infrastructure/storage/RedisSummaryAdapter.h"
#include <loguru.hpp>
#include <memory>
#include <string>

using namespace finance;

int main(int argc, char *argv[])
{
    try
    {
        // Initialize logging
        loguru::init(argc, argv);
        loguru::add_file("finance.log", loguru::Append, loguru::Verbosity_MAX);
        LOG_F(INFO, "Starting Finance Service...");

        // Load configuration
        auto configProvider = std::make_shared<infrastructure::config::ConnectionConfigProvider>("connection.json");
        if (!configProvider->loadFromFile("connection.json"))
        {
            LOG_F(ERROR, "Failed to load configuration");
            return 1;
        }

        // Initialize area branch provider
        auto areaBranchProvider = std::make_shared<infrastructure::config::AreaBranchProvider>(
            configProvider->getRedisUrl());
        if (!areaBranchProvider->loadFromFile("area_branch.json"))
        {
            LOG_F(ERROR, "Failed to load area-branch mapping");
            return 1;
        }

        // Initialize repository
        auto repository = std::make_shared<infrastructure::storage::RedisSummaryAdapter>(
            configProvider,
            areaBranchProvider);

        // Create packet handler factory
        auto packetHandler = std::make_shared<infrastructure::network::PacketProcessorFactory>(
            repository, areaBranchProvider);

        // Create and initialize service
        application::FinanceService service(packetHandler, repository, areaBranchProvider);
        auto status = service.initialize("connection.json");
        if (!status.isOk())
        {
            LOG_F(ERROR, "Failed to initialize service: %s", status.message().c_str());
            return 1;
        }

        // Run service
        status = service.run();
        if (!status.isOk())
        {
            LOG_F(ERROR, "Service error: %s", status.message().c_str());
            return 1;
        }

        // Graceful shutdown
        service.shutdown();
        LOG_F(INFO, "Service shutdown complete");
        return 0;
    }
    catch (const std::exception &ex)
    {
        LOG_F(ERROR, "Unhandled exception: %s", ex.what());
        return 1;
    }
    catch (...)
    {
        LOG_F(ERROR, "Unknown error occurred");
        return 1;
    }
}