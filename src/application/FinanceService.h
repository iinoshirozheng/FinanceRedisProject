#pragma once

#include "../domain/FinanceDataStructures.h"
#include "../domain/IFinanceRepository.h"
#include "../domain/IPacketHandler.h"
#include "../infrastructure/storage/RedisSummaryAdapter.h"
#include "../infrastructure/storage/AreaBranchAdapter.h"
#include <memory>
#include <string>
#include <map>
#include <mutex>
#include <vector>
#include <optional>

namespace finance
{
    namespace application
    {
        // Forward declarations
        class FinanceServiceFactory;

        // Simple constructor for the basic FinanceService that only uses RedisSummaryAdapter
        class FinanceService
        {
        public:
            /**
             * @brief Construct a new Finance Service object
             * @param repository storage adapter for finance data
             * @param areaBranchRepo area branch adapter
             */
            explicit FinanceService(
                std::shared_ptr<finance::infrastructure::storage::RedisSummaryAdapter> repository,
                std::shared_ptr<finance::infrastructure::storage::AreaBranchAdapter> areaBranchRepo = nullptr);

            /**
             * @brief Save a finance summary
             * @param summary The summary data to save
             * @return true if successful, false otherwise
             */
            bool saveSummary(const domain::SummaryData &summary);

            /**
             * @brief Get a finance summary by area center
             * @param areaCenter The area center to retrieve
             * @return The summary data if found
             */
            std::optional<domain::SummaryData> getSummary(const std::string &areaCenter);

            /**
             * @brief Update an existing finance summary
             * @param data The updated summary data
             * @param areaCenter The area center to update
             * @return true if successful, false otherwise
             */
            bool updateSummary(const domain::SummaryData &data, const std::string &areaCenter);

            /**
             * @brief Delete a finance summary
             * @param areaCenter The area center to delete
             * @return true if successful, false otherwise
             */
            bool deleteSummary(const std::string &areaCenter);

            /**
             * @brief Get all finance summaries
             * @return Vector of all summary data
             */
            std::vector<domain::SummaryData> getAllSummaries();

            /**
             * @brief Load all summary data
             */
            void loadAllSummaryData();

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
        };

        /**
         * @brief Factory for creating finance service related objects
         */
        class FinanceServiceFactory
        {
        public:
            /**
             * @brief Construct a new Finance Service Factory object
             * @param service The finance service instance
             */
            explicit FinanceServiceFactory(std::shared_ptr<FinanceService> service);

            /**
             * @brief Create a Finance Bill Handler object
             * @return std::shared_ptr<domain::IFinanceBillHandler>
             */
            std::shared_ptr<domain::IFinanceBillHandler> createFinanceBillHandler();

        private:
            std::shared_ptr<FinanceService> service;
        };
    } // namespace application
} // namespace finance