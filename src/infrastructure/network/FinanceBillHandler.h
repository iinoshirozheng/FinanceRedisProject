#pragma once

#include "../../domain/FinanceDataStructures.h"
#include "../../domain/IPacketHandler.h"
#include "../../application/FinanceService.h"
#include <memory>
#include <string>
#include <cstring>
#include <boost/algorithm/string.hpp>
#include <map>
#include <set>
#include <vector>

namespace finance
{
    namespace infrastructure
    {
        namespace network
        {
            /**
             * @brief Implementation of the IPacketHandler interface for financial bills
             */
            class FinanceBillHandler : public domain::IPacketHandler
            {
            public:
                explicit FinanceBillHandler(std::shared_ptr<application::FinanceService> service);
                ~FinanceBillHandler() override = default;

                bool processBill(const domain::FinanceBill &bill) override;
                bool processData(const domain::Hcrtm01Data &hcrtm01, const std::string &systemHeader) override;
                bool processData(const domain::Hcrtm05pData &hcrtm05p) override;

            private:
                std::shared_ptr<application::FinanceService> service;
                std::map<std::string, std::string> followingBrokerIds;
                std::set<std::string> backofficeIds;
                std::vector<std::string> branches;

                // Convert and handle HCRTM01 data
                void handleHcrtm01(const domain::Hcrtm01Data &hcrtm01, const std::string &systemHeader);

                // Convert and handle HCRTM05P data
                void handleHcrtm05p(const domain::Hcrtm05pData &hcrtm05p);

                // Update the company-wide summary for a stock ID
                void updateCompanySummary(const std::string &stockId);

                // Update the area-level summary for a branch and stock
                void updateAreaSummary(const std::string &branchId, const std::string &stockId);

                // Extract string from char array and trim right spaces
                std::string extractString(const char *data, size_t size);

                // Initialize office IDs and branch mappings
                void initializeOfficeIds();
            };

        } // namespace network
    } // namespace infrastructure
} // namespace finance