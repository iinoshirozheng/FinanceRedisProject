#include "../infrastructure/network/TransactionProcessor.hpp"
#include "../infrastructure/network/FinancePackageHandler.hpp"
#include "../infrastructure/storage/RedisSummaryAdapter.hpp"
#include "../infrastructure/config/ConnectionConfigProvider.hpp"
#include "../infrastructure/config/AreaBranchProvider.hpp"
#include "../application/FinanceService.hpp"
#include "../domain/Result.hpp"
#include <memory>
#include <iostream>
#include <filesystem>
#include <loguru.hpp>

using namespace finance;

int main(int argc, char *argv[])
{
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("finance.log", loguru::Append, loguru::Verbosity_MAX);

    try
    {
        LOG_F(INFO, "Starting Finance Service...");

        // Check if config files exist
        std::filesystem::path connectionConfig("connection.json");
        std::filesystem::path areaBranchConfig("area_branch.json");

        if (!std::filesystem::exists(connectionConfig))
        {
            LOG_F(ERROR, "Required config file not found: %s", connectionConfig.c_str());
            std::cerr << "ERROR: Required config file not found: " << connectionConfig << "\n";
            return 1;
        }

        if (!std::filesystem::exists(areaBranchConfig))
        {
            LOG_F(ERROR, "Required config file not found: %s", areaBranchConfig.c_str());
            std::cerr << "ERROR: Required config file not found: " << areaBranchConfig << "\n";
            return 1;
        }

        // Load configuration files before connecting to Redis
        LOG_F(INFO, "Loading configuration files...");

        // Load connection config
        if (!infrastructure::config::ConnectionConfigProvider::loadFromFile(connectionConfig.string()))
        {
            LOG_F(ERROR, "Failed to load connection configuration");
            std::cerr << "ERROR: Failed to load connection configuration\n";
            return 1;
        }

        // Load area branch config
        if (!infrastructure::config::AreaBranchProvider::loadFromFile(areaBranchConfig.string()))
        {
            LOG_F(ERROR, "Failed to load area branch configuration");
            std::cerr << "ERROR: Failed to load area branch configuration\n";
            return 1;
        }

        LOG_F(INFO, "Configurations loaded successfully");
        LOG_F(INFO, "Redis URL: %s", infrastructure::config::ConnectionConfigProvider::redisUri().c_str());
        LOG_F(INFO, "Server Port: %d", infrastructure::config::ConnectionConfigProvider::serverPort());

        // Create repository with exception handling
        std::shared_ptr<infrastructure::storage::RedisSummaryAdapter> redisRepo;
        try
        {
            LOG_F(INFO, "Creating Redis adapter...");
            redisRepo = std::make_shared<infrastructure::storage::RedisSummaryAdapter>();
        }
        catch (const std::exception &e)
        {
            LOG_F(ERROR, "Failed to create Redis adapter: %s", e.what());
            std::cerr << "ERROR: Redis initialization failed: " << e.what() << "\n";
            std::cerr << "Please ensure Redis server is available.\n";
            return 1;
        }

        // Create transaction processor and register handlers
        LOG_F(INFO, "Setting up transaction handlers...");
        auto processor = std::make_shared<infrastructure::network::TransactionProcessor>();

        // Register handlers with proper repository injection
        processor->registerHandler("ELD001",
                                   std::make_unique<infrastructure::network::Hcrtm01Handler>(redisRepo));
        processor->registerHandler("ELD002",
                                   std::make_unique<infrastructure::network::Hcrtm05pHandler>(redisRepo));
        LOG_F(INFO, "Transaction handlers registered");

        // Create the FinanceService
        LOG_F(INFO, "Creating Finance Service...");
        application::FinanceService financeService(redisRepo, processor);

        // Initialize the service
        LOG_F(INFO, "Initializing Finance Service...");
        auto initResult = financeService.initialize();
        if (initResult.is_err())
        {
            LOG_F(ERROR, "Failed to initialize Finance Service: %s", initResult.unwrap_err().message.c_str());
            std::cerr << "ERROR: " << initResult.unwrap_err().message << "\n";
            return 1;
        }

        // Run the service
        LOG_F(INFO, "Running Finance Service...");
        auto runResult = financeService.run();
        if (runResult.is_err())
        {
            LOG_F(ERROR, "Finance Service failed: %s", runResult.unwrap_err().message.c_str());
            std::cerr << "ERROR: " << runResult.unwrap_err().message << "\n";
            return 1;
        }

        return 0;
    }
    catch (const std::exception &e)
    {
        LOG_F(ERROR, "Unhandled exception: %s", e.what());
        std::cerr << "FATAL ERROR: " << e.what() << "\n";
        return 1;
    }
    catch (...)
    {
        LOG_F(ERROR, "Unknown error occurred");
        std::cerr << "FATAL ERROR: Unknown exception caught\n";
        return 1;
    }
}
