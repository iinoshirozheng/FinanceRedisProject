#include "../application/FinanceService.h"
#include "../infrastructure/config/ConnectionConfigProvider.hpp"
#include "../infrastructure/config/AreaBranchProvider.hpp"
#include <loguru.hpp>
#include <memory>
#include <string>

using namespace finance;

int main(int argc, char *argv[])
{
    try
    {
        // Initialize logging with async mode
        loguru::init(argc, argv);
        loguru::add_file("finance.log", loguru::Append, loguru::Verbosity_MAX);
        LOG_F(INFO, "Starting Finance Service...");

        // Load configuration
        if (!infrastructure::config::ConnectionConfigProvider::loadFromFile("connection.json"))
        {
            LOG_F(ERROR, "Failed to load configuration");
            return 1;
        }

        // Initialize area branch provider
        if (!infrastructure::config::AreaBranchProvider::loadFromFile("area_branch.json"))
        {
            LOG_F(ERROR, "Failed to load area-branch mapping");
            return 1;
        }

        // Create and initialize service
        application::FinanceService service;
        auto status = service.initialize("connection.json");
        if (!status.isOk())
        {
            LOG_F(ERROR, "Failed to initialize service: %s", status.message().c_str());
            return 1;
        }

        // Run service
        service.run();

        // Cleanup
        loguru::flush(); // 确保所有日志都被写入
        return 0;
    }
    catch (const std::exception &e)
    {
        LOG_F(FATAL, "Unhandled exception: %s", e.what());
        return 1;
    }
}