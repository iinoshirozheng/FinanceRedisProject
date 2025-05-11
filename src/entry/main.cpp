#include "../infrastructure/network/TcpServiceAdapter.hpp"
#include "../infrastructure/network/TransactionProcessor.hpp"
#include "../infrastructure/network/FinancePackageHandler.hpp"
#include "../infrastructure/storage/RedisSummaryAdapter.hpp"
#include "../domain/Result.hpp"
#include <memory>
#include <iostream>
#include <thread>
#include <chrono>
#include <loguru.hpp>

using namespace finance;

int main(int argc, char *argv[])
{
    // Initialize logging
    loguru::init(argc, argv);
    loguru::add_file("finance.log", loguru::Append, loguru::Verbosity_MAX);

    try
    {
        // Create repository
        auto redisRepo = std::make_shared<infrastructure::storage::RedisSummaryAdapter>();
        auto initResult = redisRepo->init();
        if (initResult.is_err())
        {
            LOG_F(ERROR, "Failed to initialize Redis: %s", initResult.unwrap_err().message.c_str());
            return 1;
        }

        // Create transaction processor and register handlers
        auto processor = std::make_shared<infrastructure::network::TransactionProcessor>();

        // Register handlers with proper repository injection
        processor->registerHandler("ELD001",
                                   std::make_unique<infrastructure::network::Hcrtm01Handler>(redisRepo));
        processor->registerHandler("ELD002",
                                   std::make_unique<infrastructure::network::Hcrtm05pHandler>(redisRepo));

        // Create TCP service adapter with the processor as the handler
        infrastructure::network::TcpServiceAdapter tcpAdapter(processor, redisRepo);

        // Start the TCP service
        if (!tcpAdapter.start())
        {
            LOG_F(ERROR, "Failed to start TCP service");
            return 1;
        }

        LOG_F(INFO, "Finance service started. Press Ctrl+C to stop.");

        // Wait for signal to stop
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        return 0;
    }
    catch (const std::exception &e)
    {
        LOG_F(ERROR, "Error: %s", e.what());
        return 1;
    }
}
