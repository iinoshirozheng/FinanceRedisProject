#include "FinanceService.h"
#include "../domain/FinanceUtils.hpp"
#include "../infrastructure/network/FinanceBillHandler.h"
#include <iostream>
#include <algorithm>

namespace finance
{
    namespace application
    {

        FinanceService::FinanceService(
            std::shared_ptr<finance::infrastructure::storage::RedisSummaryAdapter> repository,
            std::shared_ptr<finance::infrastructure::storage::AreaBranchAdapter> areaBranchRepo)
            : repository(repository), areaBranchRepo(areaBranchRepo)
        {
        }

        bool FinanceService::saveSummary(const domain::SummaryData &summary)
        {
            return repository->saveSummary(summary);
        }

        std::optional<domain::SummaryData> FinanceService::getSummary(const std::string &areaCenter)
        {
            return repository->getSummary(areaCenter);
        }

        bool FinanceService::updateSummary(const domain::SummaryData &data, const std::string &areaCenter)
        {
            return repository->updateSummary(data, areaCenter);
        }

        bool FinanceService::deleteSummary(const std::string &areaCenter)
        {
            return repository->deleteSummary(areaCenter);
        }

        std::vector<domain::SummaryData> FinanceService::getAllSummaries()
        {
            return repository->loadAllData();
        }

        void FinanceService::loadAllSummaryData()
        {
            // Just load the data to verify it's accessible
            auto data = repository->loadAllData();
            std::cout << "Loaded " << data.size() << " summary records from Redis" << std::endl;
        }

        // FinanceServiceFactory implementation
        FinanceServiceFactory::FinanceServiceFactory(std::shared_ptr<FinanceService> service)
            : service(service)
        {
        }

        std::shared_ptr<domain::IFinanceBillHandler> FinanceServiceFactory::createFinanceBillHandler()
        {
            // Create and return a new FinanceBillHandler instance
            return std::make_shared<infrastructure::network::FinanceBillHandler>(service);
        }

    } // namespace application
} // namespace finance