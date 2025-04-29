#pragma once

#include "../domain/FinanceDataStructures.h"
#include "../domain/IFinanceRepository.h"
#include "../domain/IPacketHandler.h"
#include "../infrastructure/storage/RedisSummaryAdapter.h"
#include "../infrastructure/storage/AreaBranchAdapter.h"
#include "../infrastructure/config/ConfigAdapter.hpp"
#include "../infrastructure/network/TcpServiceAdapter.h"
#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <optional>
#include <csignal>
#include <thread>

namespace finance
{
    namespace application
    {

        // Simple constructor for the basic FinanceService that only uses RedisSummaryAdapter
        class FinanceService
        {
        public:
            /**
             * @brief Initialize the finance service
             * @param argc Command line argument count
             * @param argv Command line arguments
             * @return true if successful, false otherwise
             */
            bool Initialize(int argc, char **argv);

            /**
             * @brief Run the finance service
             * @return true if successful, false otherwise
             */
            bool Run();

            /**
             * @brief Stop the finance service
             */
            void Stop();

            /**
             * @brief Get the Area Branch Repository
             * @return The area branch repository instance
             */
            std::shared_ptr<finance::infrastructure::storage::AreaBranchAdapter> getAreaBranchRepo() const
            {
                return areaBranchRepo;
            }

        private:
            std::shared_ptr<finance::infrastructure::storage::RedisSummaryAdapter> repository;
            std::shared_ptr<finance::infrastructure::storage::AreaBranchAdapter> areaBranchRepo;
            std::unique_ptr<infrastructure::network::TcpServiceAdapter> tcpService;
            volatile std::sig_atomic_t signalStatus = 0;
            domain::ConfigData config;
        };
    } // namespace application
} // namespace finance