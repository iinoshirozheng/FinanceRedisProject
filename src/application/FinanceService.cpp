#include "FinanceService.h"
#include "../common/FinanceUtils.hpp"
#include <iostream>
#include <algorithm>
#include <csignal>

namespace finance
{
    namespace application
    {
        // Static member for signal handling
        static FinanceService *g_service = nullptr;

        bool FinanceService::Initialize(int argc, char **argv)
        {
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
                config = configProvider->getConfig();
                config.initializeIndices = initIndices;

                // Initialize area branch mapping
                areaBranchRepo = std::make_shared<finance::infrastructure::storage::AreaBranchAdapter>(config.redisUrl);
                if (!areaBranchRepo->loadFromFile("area_branch.json"))
                {
                    LOG_F(ERROR, "Failed to load area-branch mapping");
                    return false;
                }

                // Initialize Redis repository
                LOG_F(INFO, "Connecting to Redis at %s", config.redisUrl.c_str());
                repository = std::make_shared<finance::infrastructure::storage::RedisSummaryAdapter>(
                    config.redisUrl);

                if (config.initializeIndices)
                {
                    LOG_F(INFO, "Initializing search indices");
                    repository->createIndex();
                }

                return true;
            }
            catch (const std::exception &ex)
            {
                LOG_F(ERROR, "Unhandled exception: %s", ex.what());
                return false;
            }
        }

        bool FinanceService::Run()
        {
            try
            {
                // Create and start TCP service
                LOG_F(INFO, "Starting TCP service on port %d", config.serverPort);
                tcpService = std::make_unique<infrastructure::network::TcpServiceAdapter>(
                    config.serverPort,
                    nullptr); // TODO: Add packet handler

                if (!tcpService->start())
                {
                    LOG_F(ERROR, "Failed to start TCP service");
                    return false;
                }

                // Set up signal handling
                g_service = this;
                std::signal(SIGINT, [](int signal)
                            {
                    if (g_service) g_service->signalStatus = signal; });
                std::signal(SIGTERM, [](int signal)
                            {
                    if (g_service) g_service->signalStatus = signal; });

                // Main loop
                LOG_F(INFO, "Finance System running, press Ctrl+C to stop");
                while (signalStatus == 0)
                {
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }

                return true;
            }
            catch (const std::exception &ex)
            {
                LOG_F(ERROR, "Unhandled exception: %s", ex.what());
                return false;
            }
        }

        void FinanceService::Stop()
        {
            if (tcpService)
            {
                tcpService->stop();
            }
            g_service = nullptr;
        }

    } // namespace application
} // namespace finance