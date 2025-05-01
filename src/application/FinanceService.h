#pragma once

#include "../domain/FinanceDataStructure.h"
#include "../domain/IFinanceRepository.h"
#include "../domain/IPacketHandler.h"
#include "../domain/Status.h"
#include "../infrastructure/config/AreaBranchProvider.hpp"
#include "../infrastructure/config/ConnectionConfigProvider.hpp"
#include "../infrastructure/network/TcpServiceAdapter.h"
#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <optional>
#include <csignal>
#include <thread>
#include <atomic>

namespace finance
{
    namespace application
    {
        class FinanceService
        {
        public:
            FinanceService(
                std::shared_ptr<domain::IPackageHandler> packetHandler,
                std::shared_ptr<domain::IFinanceRepository<domain::SummaryData>> repository,
                std::shared_ptr<infrastructure::config::AreaBranchProvider> areaBranchProvider);

            // Initialize the service with configuration
            domain::Status initialize(const std::string &configPath);

            // Start the service main loop
            domain::Status run();

            // Stop the service gracefully
            void shutdown();

            /**
             * @brief Get the Area Branch Repository
             * @return The area branch repository instance
             */
            std::shared_ptr<infrastructure::config::AreaBranchProvider> getAreaBranchRepo() const
            {
                return areaBranchProvider_;
            }

        private:
            std::shared_ptr<domain::IPackageHandler> packetHandler_;
            std::shared_ptr<domain::IFinanceRepository<domain::SummaryData>> repository_;
            std::shared_ptr<infrastructure::config::AreaBranchProvider> areaBranchProvider_;
            std::unique_ptr<infrastructure::network::TcpServiceAdapter> tcpService_;
            std::atomic<int> signalStatus_{0};
        };
    } // namespace application
} // namespace finance