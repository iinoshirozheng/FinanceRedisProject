#include "FinanceService.h"
#include "../common/FinanceUtils.hpp"
#include "../infrastructure/network/FinancePackageHandler.h"
#include "../infrastructure/network/TcpServiceAdapter.h"
#include "../infrastructure/config/ConnectionConfigProvider.hpp"
#include <iostream>
#include <algorithm>
#include <csignal>
#include <loguru.hpp>

namespace finance::application
{
    // Static member for signal handling
    static FinanceService *g_service = nullptr;

    FinanceService::FinanceService(
        std::shared_ptr<domain::IPackageHandler> packetHandler,
        std::shared_ptr<domain::IFinanceRepository<domain::SummaryData>> repository,
        std::shared_ptr<infrastructure::config::AreaBranchProvider> areaBranchProvider)
        : packetHandler_(std::move(packetHandler)), repository_(std::move(repository)), areaBranchProvider_(std::move(areaBranchProvider))
    {
    }

    domain::Status FinanceService::initialize(const std::string &configPath)
    {
        try
        {
            // Load configuration
            auto configProvider = std::make_unique<infrastructure::config::ConnectionConfigProvider>(configPath);
            if (!configProvider->loadFromFile(configPath))
            {
                return domain::Status::error(
                    domain::Status::Code::ERROR,
                    "Failed to load configuration file: " + configPath);
            }

            // Initialize area branch mapping
            if (!areaBranchProvider_->loadFromFile("area_branch.json"))
            {
                return domain::Status::error(
                    domain::Status::Code::ERROR,
                    "Failed to load area-branch mapping");
            }

            // Create and start TCP service
            LOG_F(INFO, "Starting TCP service on port %d", configProvider->getServerPort());
            tcpService_ = std::make_unique<infrastructure::network::TcpServiceAdapter>(
                configProvider->getServerPort(),
                packetHandler_);

            return domain::Status::ok();
        }
        catch (const std::exception &ex)
        {
            return domain::Status::error(
                domain::Status::Code::INITIALIZATION_ERROR,
                std::string("Initialization failed: ") + ex.what());
        }
    }

    domain::Status FinanceService::run()
    {
        try
        {
            if (!tcpService_->start())
            {
                return domain::Status::error(
                    domain::Status::Code::RUNTIME_ERROR,
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
            return domain::Status::error(
                domain::Status::Code::RUNTIME_ERROR,
                std::string("Runtime error: ") + ex.what());
        }
    }

    void FinanceService::shutdown()
    {
        if (tcpService_)
        {
            tcpService_->stop();
        }
        g_service = nullptr;
    }

} // namespace finance::application