#include "FinanceService.h"
#include "../infrastructure/network/FinancePackageHandler.h"
#include "../infrastructure/network/TcpServiceAdapter.h"
#include "../infrastructure/config/ConnectionConfigProvider.hpp"
#include "../infrastructure/config/AreaBranchProvider.hpp"
#include <iostream>
#include <algorithm>
#include <csignal>
#include <loguru.hpp>
#include <set>

namespace finance::application
{
    // Static member for signal handling
    static FinanceService *g_service = nullptr;

    domain::Status FinanceService::initialize()
    {
        if (isInitialized_)
        {
            return domain::Status::error(
                domain::Status::Code::InitializationError,
                "Service is already initialized");
        }

        try
        {
            domain::Status status;
            // Initialize repository
            if (!repository_.init())
            {
                status = domain::Status::error(
                    domain::Status::Code::InitializationError,
                    "Failed to initialize Redis repository");
                return status;
            }

            // Create and start TCP service
            LOG_F(INFO, "Starting TCP service on port %d", infrastructure::config::ConnectionConfigProvider::serverPort());
            tcpService_ = std::make_unique<infrastructure::network::TcpServiceAdapter>(&packetHandler_);

            // Load all data from Redis
            LOG_F(INFO, "Loading data from Redis...");
            status = repository_.loadAllFromRedis();
            if (!status.isOk())
            {
                LOG_F(ERROR, "%s", status.toString().c_str());
                return status;
            }

            LOG_F(INFO, "Loaded all data from Redis");

            // Update company summaries for all stocks
            std::set<std::string, std::less<>> processedStocks;
            for (const auto &[stockId, data] : repository_.getAllMapped())
            {
                if (processedStocks.insert(stockId).second)
                {
                    if (!repository_.updateCompanySummary(stockId))
                    {
                        LOG_F(WARNING, "Failed to update company summary for stock: %s", stockId.c_str());
                    }
                }
            }

            isInitialized_ = true;
            return domain::Status::ok();
        }
        catch (const std::exception &ex)
        {
            return domain::Status::error(
                domain::Status::Code::InitializationError,
                std::string("Initialization failed: ") + ex.what());
        }
    }

    domain::Status FinanceService::run()
    {
        if (!isInitialized_)
        {
            return domain::Status::error(
                domain::Status::Code::RuntimeError,
                "Service is not initialized");
        }

        if (isRunning_)
        {
            return domain::Status::error(
                domain::Status::Code::RuntimeError,
                "Service is already running");
        }

        try
        {
            if (!tcpService_->start())
            {
                return domain::Status::error(
                    domain::Status::Code::RuntimeError,
                    "Failed to start TCP service");
            }

            // Set up signal handling
            g_service = this;
            std::signal(SIGINT, [](int signal)
                        {
                if (g_service) g_service->signalStatus_ = signal; });
            std::signal(SIGTERM, [](int signal)
                        {
                if (g_service) g_service->signalStatus_ = signal; });

            isRunning_ = true;

            // Main loop
            LOG_F(INFO, "Finance System running, press Ctrl+C to stop");
            while (signalStatus_ == 0)
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            return domain::Status::ok();
        }
        catch (const std::exception &ex)
        {
            isRunning_ = false;
            return domain::Status::error(
                domain::Status::Code::RuntimeError,
                std::string("Runtime error: ") + ex.what());
        }
    }

    void FinanceService::shutdown()
    {
        if (tcpService_)
        {
            tcpService_->stop();
        }

        isRunning_ = false;
        g_service = nullptr;
    }
} // namespace finance::application