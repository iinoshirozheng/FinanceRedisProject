#include "FinanceBillHandler.h"
#include "../../domain/FinanceUtils.hpp"
#include <boost/algorithm/string.hpp>
#include <Poco/DateTime.h>
#include <Poco/Timezone.h>

namespace finance
{
    namespace infrastructure
    {
        namespace network
        {
            FinanceBillHandler::FinanceBillHandler(std::shared_ptr<application::FinanceService> service)
                : service(service)
            {
            }

            bool FinanceBillHandler::processBill(const domain::FinanceBill &bill)
            {
                auto messageType = domain::FinanceUtils::determineMessageType(bill);

                if (messageType == domain::MessageType::HCRTM01)
                {
                    domain::Hcrtm01Data hcrtm01;
                    // Convert bill to HCRTM01 data
                    std::memcpy(hcrtm01.brokerId, bill.data.c_str(), sizeof(hcrtm01.brokerId));
                    std::memcpy(hcrtm01.areaCenter, bill.data.c_str() + 4, sizeof(hcrtm01.areaCenter));
                    std::memcpy(hcrtm01.stockId, bill.data.c_str() + 7, sizeof(hcrtm01.stockId));
                    std::memcpy(hcrtm01.marginAmount, bill.data.c_str() + 13, sizeof(hcrtm01.marginAmount));
                    std::memcpy(hcrtm01.marginBuyOrderAmount, bill.data.c_str() + 24, sizeof(hcrtm01.marginBuyOrderAmount));
                    std::memcpy(hcrtm01.marginSellMatchAmount, bill.data.c_str() + 35, sizeof(hcrtm01.marginSellMatchAmount));
                    std::memcpy(hcrtm01.marginQty, bill.data.c_str() + 46, sizeof(hcrtm01.marginQty));
                    std::memcpy(hcrtm01.marginBuyOrderQty, bill.data.c_str() + 52, sizeof(hcrtm01.marginBuyOrderQty));
                    std::memcpy(hcrtm01.marginSellMatchQty, bill.data.c_str() + 58, sizeof(hcrtm01.marginSellMatchQty));
                    std::memcpy(hcrtm01.shortAmount, bill.data.c_str() + 64, sizeof(hcrtm01.shortAmount));
                    std::memcpy(hcrtm01.shortSellOrderAmount, bill.data.c_str() + 75, sizeof(hcrtm01.shortSellOrderAmount));
                    std::memcpy(hcrtm01.shortBuyMatchAmount, bill.data.c_str() + 86, sizeof(hcrtm01.shortBuyMatchAmount));
                    std::memcpy(hcrtm01.shortQty, bill.data.c_str() + 97, sizeof(hcrtm01.shortQty));
                    std::memcpy(hcrtm01.shortSellOrderQty, bill.data.c_str() + 103, sizeof(hcrtm01.shortSellOrderQty));
                    std::memcpy(hcrtm01.shortBuyMatchQty, bill.data.c_str() + 109, sizeof(hcrtm01.shortBuyMatchQty));

                    return processData(hcrtm01, bill.systemHeader);
                }
                else if (messageType == domain::MessageType::HCRTM05P)
                {
                    domain::Hcrtm05pData hcrtm05p;
                    // Convert bill to HCRTM05P data
                    std::memcpy(hcrtm05p.brokerId, bill.data.c_str() + 1, sizeof(hcrtm05p.brokerId));
                    std::memcpy(hcrtm05p.stockId, bill.data.c_str() + 4, sizeof(hcrtm05p.stockId));
                    std::memcpy(hcrtm05p.financingCompany, bill.data.c_str() + 10, sizeof(hcrtm05p.financingCompany));
                    std::memcpy(hcrtm05p.account, bill.data.c_str() + 14, sizeof(hcrtm05p.account));
                    std::memcpy(hcrtm05p.marginBuyMatchQty, bill.data.c_str() + 21, sizeof(hcrtm05p.marginBuyMatchQty));
                    std::memcpy(hcrtm05p.shortSellMatchQty, bill.data.c_str() + 27, sizeof(hcrtm05p.shortSellMatchQty));
                    std::memcpy(hcrtm05p.dayTradeMarginMatchQty, bill.data.c_str() + 33, sizeof(hcrtm05p.dayTradeMarginMatchQty));
                    std::memcpy(hcrtm05p.dayTradeShortMatchQty, bill.data.c_str() + 39, sizeof(hcrtm05p.dayTradeShortMatchQty));
                    std::memcpy(hcrtm05p.marginBuyOffsetQty, bill.data.c_str() + 45, sizeof(hcrtm05p.marginBuyOffsetQty));
                    std::memcpy(hcrtm05p.shortSellOffsetQty, bill.data.c_str() + 51, sizeof(hcrtm05p.shortSellOffsetQty));

                    return processData(hcrtm05p);
                }

                return false;
            }

            bool FinanceBillHandler::processData(const domain::Hcrtm01Data &hcrtm01, const std::string &systemHeader)
            {
                try
                {
                    handleHcrtm01(hcrtm01, systemHeader);
                    return true;
                }
                catch (const std::exception &)
                {
                    return false;
                }
            }

            bool FinanceBillHandler::processData(const domain::Hcrtm05pData &hcrtm05p)
            {
                try
                {
                    handleHcrtm05p(hcrtm05p);
                    return true;
                }
                catch (const std::exception &)
                {
                    return false;
                }
            }

            void FinanceBillHandler::handleHcrtm01(const domain::Hcrtm01Data &hcrtm01, const std::string &systemHeader)
            {
                // Create summary data from HCRTM01
                domain::SummaryData summary;
                summary.stockId = std::string(hcrtm01.stockId, sizeof(hcrtm01.stockId));
                summary.areaCenter = std::string(hcrtm01.areaCenter, sizeof(hcrtm01.areaCenter));

                // Calculate available amounts
                auto marginAmount = domain::FinanceUtils::backOfficeToInt(std::string(hcrtm01.marginAmount, sizeof(hcrtm01.marginAmount)));
                auto marginBuyOrderAmount = domain::FinanceUtils::backOfficeToInt(std::string(hcrtm01.marginBuyOrderAmount, sizeof(hcrtm01.marginBuyOrderAmount)));
                auto marginSellMatchAmount = domain::FinanceUtils::backOfficeToInt(std::string(hcrtm01.marginSellMatchAmount, sizeof(hcrtm01.marginSellMatchAmount)));
                auto marginQty = domain::FinanceUtils::backOfficeToInt(std::string(hcrtm01.marginQty, sizeof(hcrtm01.marginQty)));
                auto marginBuyOrderQty = domain::FinanceUtils::backOfficeToInt(std::string(hcrtm01.marginBuyOrderQty, sizeof(hcrtm01.marginBuyOrderQty)));
                auto marginSellMatchQty = domain::FinanceUtils::backOfficeToInt(std::string(hcrtm01.marginSellMatchQty, sizeof(hcrtm01.marginSellMatchQty)));
                auto shortAmount = domain::FinanceUtils::backOfficeToInt(std::string(hcrtm01.shortAmount, sizeof(hcrtm01.shortAmount)));
                auto shortSellOrderAmount = domain::FinanceUtils::backOfficeToInt(std::string(hcrtm01.shortSellOrderAmount, sizeof(hcrtm01.shortSellOrderAmount)));
                auto shortQty = domain::FinanceUtils::backOfficeToInt(std::string(hcrtm01.shortQty, sizeof(hcrtm01.shortQty)));
                auto shortSellOrderQty = domain::FinanceUtils::backOfficeToInt(std::string(hcrtm01.shortSellOrderQty, sizeof(hcrtm01.shortSellOrderQty)));

                // Calculate available amounts
                summary.marginAvailableAmount = marginAmount - marginBuyOrderAmount + marginSellMatchAmount;
                summary.marginAvailableQty = marginQty - marginBuyOrderQty + marginSellMatchQty;
                summary.shortAvailableAmount = shortAmount - shortSellOrderAmount;
                summary.shortAvailableQty = shortQty - shortSellOrderQty;

                // Save or update the summary
                if (!service->saveSummary(summary))
                {
                    service->updateSummary(summary, summary.areaCenter);
                }

                // Update company-wide summary
                updateCompanySummary(summary.stockId);
            }

            void FinanceBillHandler::handleHcrtm05p(const domain::Hcrtm05pData &hcrtm05p)
            {
                // Create summary data from HCRTM05P
                domain::SummaryData summary;
                summary.stockId = std::string(hcrtm05p.stockId, sizeof(hcrtm05p.stockId));
                summary.areaCenter = std::string(hcrtm05p.brokerId, sizeof(hcrtm05p.brokerId));

                // Calculate offset quantities
                auto marginBuyOffsetQty = domain::FinanceUtils::backOfficeToInt(std::string(hcrtm05p.marginBuyOffsetQty, sizeof(hcrtm05p.marginBuyOffsetQty)));
                auto shortSellOffsetQty = domain::FinanceUtils::backOfficeToInt(std::string(hcrtm05p.shortSellOffsetQty, sizeof(hcrtm05p.shortSellOffsetQty)));

                // Get existing data if available
                auto existingSummary = service->getSummary(summary.areaCenter);
                if (existingSummary)
                {
                    // Preserve existing data
                    summary = *existingSummary;
                }

                // Update quantities
                summary.marginAvailableQty += marginBuyOffsetQty;
                summary.shortAvailableQty += shortSellOffsetQty;
                summary.afterMarginAvailableQty += marginBuyOffsetQty;
                summary.afterShortAvailableQty += shortSellOffsetQty;

                // Save or update the summary
                if (!service->saveSummary(summary))
                {
                    service->updateSummary(summary, summary.areaCenter);
                }

                // Update area-level summary
                updateAreaSummary(summary.areaCenter, summary.stockId);
            }

            void FinanceBillHandler::updateAreaSummary(const std::string &branchId, const std::string &stockId)
            {
                // 1) Find the area center for this branch
                auto areaCenter = service->getAreaBranchRepo()->getAreaForBranch(branchId);
                if (areaCenter.empty())
                    return;

                // 2) Construct area-level key
                std::string areaKey = "summary:" + areaCenter + ":" + stockId;

                // 3) Get existing area summary or create new one
                domain::SummaryData areaSummary;
                auto existingAreaSummary = service->getSummary(areaKey);
                if (existingAreaSummary)
                {
                    areaSummary = *existingAreaSummary;
                }

                // 4) Set meta fields
                areaSummary.stockId = stockId;
                areaSummary.areaCenter = areaCenter;

                // 5) Get all branches for this area and sum up their offsets
                int64_t totalBuyOffset = 0;
                int64_t totalShortOffset = 0;
                auto branches = service->getAreaBranchRepo()->getBranchesForArea(areaCenter);
                for (const auto &branch : branches)
                {
                    std::string branchKey = "summary:" + branch + ":" + stockId;
                    auto branchSummary = service->getSummary(branchKey);
                    if (branchSummary)
                    {
                        totalBuyOffset += branchSummary->marginAvailableQty;
                        totalShortOffset += branchSummary->shortAvailableQty;
                    }
                }

                // 6) Update area summary with total offsets
                areaSummary.marginAvailableQty = totalBuyOffset;
                areaSummary.shortAvailableQty = totalShortOffset;
                areaSummary.afterMarginAvailableQty = totalBuyOffset;
                areaSummary.afterShortAvailableQty = totalShortOffset;

                // 7) Save area summary
                if (!service->saveSummary(areaSummary))
                {
                    service->updateSummary(areaSummary, areaKey);
                }

                // 8) Update company-wide summary
                updateCompanySummary(stockId);
            }

            void FinanceBillHandler::updateCompanySummary(const std::string &stockId)
            {
                // Get all summaries for this stock
                auto summaries = service->getAllSummaries();

                // Create company-wide summary
                domain::SummaryData companySummary;
                companySummary.stockId = stockId;
                companySummary.areaCenter = "ALL";

                // Aggregate data from all summaries
                for (const auto &summary : summaries)
                {
                    if (summary.stockId == stockId)
                    {
                        companySummary.marginAvailableAmount += summary.marginAvailableAmount;
                        companySummary.marginAvailableQty += summary.marginAvailableQty;
                        companySummary.shortAvailableAmount += summary.shortAvailableAmount;
                        companySummary.shortAvailableQty += summary.shortAvailableQty;
                        companySummary.afterMarginAvailableAmount += summary.afterMarginAvailableAmount;
                        companySummary.afterMarginAvailableQty += summary.afterMarginAvailableQty;
                        companySummary.afterShortAvailableAmount += summary.afterShortAvailableAmount;
                        companySummary.afterShortAvailableQty += summary.afterShortAvailableQty;
                    }
                }

                // Save or update the company summary
                if (!service->saveSummary(companySummary))
                {
                    service->updateSummary(companySummary, companySummary.areaCenter);
                }
            }

            std::string FinanceBillHandler::extractString(const char *data, size_t size)
            {
                std::string result(data, size);
                boost::algorithm::trim_right(result);
                return result;
            }

        } // namespace network
    } // namespace infrastructure
} // namespace finance