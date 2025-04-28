#include "FinanceBillHandler.h"
#include "../../common/FinanceUtils.hpp"
#include "../../domain/FinanceDataStructures.h"
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

            bool FinanceBillHandler::processBill(const domain::FinanceBillMessage &bill)
            {
                auto messageType = common::FinanceUtils::transactionMessageType(bill.t_code);

                if (messageType == domain::MessageTransactionType::HCRTM01)
                {
                    domain::HCRTM01_BillQuota hcrtm01;
                    // Convert bill to HCRTM01 data
                    std::memcpy(hcrtm01.broker_id, bill.ap_data.data.hcrtm01.broker_id, sizeof(hcrtm01.broker_id));
                    std::memcpy(hcrtm01.area_center, bill.ap_data.data.hcrtm01.area_center, sizeof(hcrtm01.area_center));
                    std::memcpy(hcrtm01.stock_id, bill.ap_data.data.hcrtm01.stock_id, sizeof(hcrtm01.stock_id));
                    std::memcpy(hcrtm01.margin_amount, bill.ap_data.data.hcrtm01.margin_amount, sizeof(hcrtm01.margin_amount));
                    std::memcpy(hcrtm01.margin_buy_order_amount, bill.ap_data.data.hcrtm01.margin_buy_order_amount, sizeof(hcrtm01.margin_buy_order_amount));
                    std::memcpy(hcrtm01.margin_sell_match_amount, bill.ap_data.data.hcrtm01.margin_sell_match_amount, sizeof(hcrtm01.margin_sell_match_amount));
                    std::memcpy(hcrtm01.margin_qty, bill.ap_data.data.hcrtm01.margin_qty, sizeof(hcrtm01.margin_qty));
                    std::memcpy(hcrtm01.margin_buy_order_qty, bill.ap_data.data.hcrtm01.margin_buy_order_qty, sizeof(hcrtm01.margin_buy_order_qty));
                    std::memcpy(hcrtm01.margin_sell_match_qty, bill.ap_data.data.hcrtm01.margin_sell_match_qty, sizeof(hcrtm01.margin_sell_match_qty));
                    std::memcpy(hcrtm01.short_amount, bill.ap_data.data.hcrtm01.short_amount, sizeof(hcrtm01.short_amount));
                    std::memcpy(hcrtm01.short_sell_order_amount, bill.ap_data.data.hcrtm01.short_sell_order_amount, sizeof(hcrtm01.short_sell_order_amount));
                    std::memcpy(hcrtm01.short_qty, bill.ap_data.data.hcrtm01.short_qty, sizeof(hcrtm01.short_qty));
                    std::memcpy(hcrtm01.short_sell_order_qty, bill.ap_data.data.hcrtm01.short_sell_order_qty, sizeof(hcrtm01.short_sell_order_qty));

                    return processData(hcrtm01, std::string(bill.ap_data.system, sizeof(bill.ap_data.system)));
                }
                else if (messageType == domain::MessageTransactionType::HCRTM05P)
                {
                    domain::HCRTM05P_BillQuota hcrtm05p;
                    // Convert bill to HCRTM05P data
                    std::memcpy(hcrtm05p.broker_id, bill.ap_data.data.hcrtm05p.broker_id, sizeof(hcrtm05p.broker_id));
                    std::memcpy(hcrtm05p.stock_id, bill.ap_data.data.hcrtm05p.stock_id, sizeof(hcrtm05p.stock_id));
                    std::memcpy(hcrtm05p.financing_company, bill.ap_data.data.hcrtm05p.financing_company, sizeof(hcrtm05p.financing_company));
                    std::memcpy(hcrtm05p.account, bill.ap_data.data.hcrtm05p.account, sizeof(hcrtm05p.account));
                    std::memcpy(hcrtm05p.margin_buy_match_qty, bill.ap_data.data.hcrtm05p.margin_buy_match_qty, sizeof(hcrtm05p.margin_buy_match_qty));
                    std::memcpy(hcrtm05p.short_sell_match_qty, bill.ap_data.data.hcrtm05p.short_sell_match_qty, sizeof(hcrtm05p.short_sell_match_qty));
                    std::memcpy(hcrtm05p.day_trade_margin_match_qty, bill.ap_data.data.hcrtm05p.day_trade_margin_match_qty, sizeof(hcrtm05p.day_trade_margin_match_qty));
                    std::memcpy(hcrtm05p.day_trade_short_match_qty, bill.ap_data.data.hcrtm05p.day_trade_short_match_qty, sizeof(hcrtm05p.day_trade_short_match_qty));
                    std::memcpy(hcrtm05p.margin_buy_offset_qty, bill.ap_data.data.hcrtm05p.margin_buy_offset_qty, sizeof(hcrtm05p.margin_buy_offset_qty));
                    std::memcpy(hcrtm05p.short_sell_offset_qty, bill.ap_data.data.hcrtm05p.short_sell_offset_qty, sizeof(hcrtm05p.short_sell_offset_qty));

                    return processData(hcrtm05p);
                }

                return false;
            }

            bool FinanceBillHandler::processData(const domain::HCRTM01_BillQuota &hcrtm01, const std::string &systemHeader)
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

            bool FinanceBillHandler::processData(const domain::HCRTM05P_BillQuota &hcrtm05p)
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

            void FinanceBillHandler::handleHcrtm01(const domain::HCRTM01_BillQuota &hcrtm01, const std::string &systemHeader)
            {
                // Create summary data from HCRTM01
                domain::SummaryData summary;
                summary.stockId = std::string(hcrtm01.stock_id, sizeof(hcrtm01.stock_id));
                summary.areaCenter = std::string(hcrtm01.area_center, sizeof(hcrtm01.area_center));

                // Calculate available amounts
                auto margin_amount = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_amount, sizeof(hcrtm01.margin_amount));
                auto margin_buy_order_amount = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_buy_order_amount, sizeof(hcrtm01.margin_buy_order_amount));
                auto margin_sell_match_amount = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_sell_match_amount, sizeof(hcrtm01.margin_sell_match_amount));
                auto margin_qty = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_qty, sizeof(hcrtm01.margin_qty));
                auto margin_buy_order_qty = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_buy_order_qty, sizeof(hcrtm01.margin_buy_order_qty));
                auto margin_sell_match_qty = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_sell_match_qty, sizeof(hcrtm01.margin_sell_match_qty));
                auto short_amount = common::FinanceUtils::backOfficeToInt(hcrtm01.short_amount, sizeof(hcrtm01.short_amount));
                auto short_sell_order_amount = common::FinanceUtils::backOfficeToInt(hcrtm01.short_sell_order_amount, sizeof(hcrtm01.short_sell_order_amount));
                auto short_qty = common::FinanceUtils::backOfficeToInt(hcrtm01.short_qty, sizeof(hcrtm01.short_qty));
                auto short_sell_order_qty = common::FinanceUtils::backOfficeToInt(hcrtm01.short_sell_order_qty, sizeof(hcrtm01.short_sell_order_qty));

                // Calculate available amounts
                summary.marginAvailableAmount = margin_amount - margin_buy_order_amount + margin_sell_match_amount;
                summary.marginAvailableQty = margin_qty - margin_buy_order_qty + margin_sell_match_qty;
                summary.shortAvailableAmount = short_amount - short_sell_order_amount;
                summary.shortAvailableQty = short_qty - short_sell_order_qty;

                // Save or update the summary
                if (!service->saveSummary(summary))
                {
                    service->updateSummary(summary, summary.areaCenter);
                }

                // Update company-wide summary
                updateCompanySummary(summary.stockId);
            }

            void FinanceBillHandler::handleHcrtm05p(const domain::HCRTM05P_BillQuota &hcrtm05p)
            {
                // Create summary data from HCRTM05P
                domain::SummaryData summary;
                summary.stockId = std::string(hcrtm05p.stock_id, sizeof(hcrtm05p.stock_id));
                summary.areaCenter = std::string(hcrtm05p.broker_id, sizeof(hcrtm05p.broker_id));

                // Calculate offset quantities
                auto margin_buy_offset_qty = common::FinanceUtils::backOfficeToInt(hcrtm05p.margin_buy_offset_qty, sizeof(hcrtm05p.margin_buy_offset_qty));
                auto short_sell_offset_qty = common::FinanceUtils::backOfficeToInt(hcrtm05p.short_sell_offset_qty, sizeof(hcrtm05p.short_sell_offset_qty));

                // Get existing data if available
                auto existingSummary = service->getSummary(summary.areaCenter);
                if (existingSummary)
                {
                    // Preserve existing data
                    summary = *existingSummary;
                }

                // Update quantities
                summary.marginAvailableQty += margin_buy_offset_qty;
                summary.shortAvailableQty += short_sell_offset_qty;
                summary.afterMarginAvailableQty += margin_buy_offset_qty;
                summary.afterShortAvailableQty += short_sell_offset_qty;

                // Save or update the summary
                if (!service->saveSummary(summary))
                {
                    service->updateSummary(summary, summary.areaCenter);
                }

                // Update area-level summary
                updateAreaSummary(summary.areaCenter, summary.stockId);
            }

            void FinanceBillHandler::updateAreaSummary(const std::string &branch_id, const std::string &stock_id)
            {
                // 1) Find the area center for this branch
                auto area_center = service->getAreaBranchRepo()->getAreaForBranch(branch_id);
                if (area_center.empty())
                    return;

                // 2) Construct area-level key
                std::string area_key = "summary:" + area_center + ":" + stock_id;

                // 3) Get existing area summary or create new one
                domain::SummaryData area_summary;
                auto existingAreaSummary = service->getSummary(area_key);
                if (existingAreaSummary)
                {
                    area_summary = *existingAreaSummary;
                }

                // 4) Set meta fields
                area_summary.stockId = stock_id;
                area_summary.areaCenter = area_center;

                // 5) Get all branches for this area and sum up their offsets
                int64_t total_buy_offset = 0;
                int64_t total_short_offset = 0;
                auto branches = service->getAreaBranchRepo()->getBranchesForArea(area_center);
                for (const auto &branch : branches)
                {
                    std::string branch_key = "summary:" + branch + ":" + stock_id;
                    auto branch_summary = service->getSummary(branch_key);
                    if (branch_summary)
                    {
                        total_buy_offset += branch_summary->marginAvailableQty;
                        total_short_offset += branch_summary->shortAvailableQty;
                    }
                }

                // 6) Update area summary with total offsets
                area_summary.marginAvailableQty = total_buy_offset;
                area_summary.shortAvailableQty = total_short_offset;
                area_summary.afterMarginAvailableQty = total_buy_offset;
                area_summary.afterShortAvailableQty = total_short_offset;

                // 7) Save area summary
                if (!service->saveSummary(area_summary))
                {
                    service->updateSummary(area_summary, area_key);
                }

                // 8) Update company-wide summary
                updateCompanySummary(stock_id);
            }

            void FinanceBillHandler::updateCompanySummary(const std::string &stock_id)
            {
                // Get all summaries for this stock
                auto summaries = service->getAllSummaries();

                // Create company-wide summary
                domain::SummaryData company_summary;
                company_summary.stockId = stock_id;
                company_summary.areaCenter = "ALL";

                // Aggregate data from all summaries
                for (const auto &summary : summaries)
                {
                    if (summary.stockId == stock_id)
                    {
                        company_summary.marginAvailableAmount += summary.marginAvailableAmount;
                        company_summary.marginAvailableQty += summary.marginAvailableQty;
                        company_summary.shortAvailableAmount += summary.shortAvailableAmount;
                        company_summary.shortAvailableQty += summary.shortAvailableQty;
                        company_summary.afterMarginAvailableAmount += summary.afterMarginAvailableAmount;
                        company_summary.afterMarginAvailableQty += summary.afterMarginAvailableQty;
                        company_summary.afterShortAvailableAmount += summary.afterShortAvailableAmount;
                        company_summary.afterShortAvailableQty += summary.afterShortAvailableQty;
                    }
                }

                // Save or update the company summary
                if (!service->saveSummary(company_summary))
                {
                    service->updateSummary(company_summary, company_summary.areaCenter);
                }
            }

            bool FinanceBillHandler::handleMessage(const char *data, size_t length)
            {
                if (data == nullptr || length == 0)
                {
                    return false;
                }

                // Parse the message into a FinanceBillMessage
                domain::FinanceBillMessage bill;
                if (length < sizeof(bill))
                {
                    return false;
                }
                std::memcpy(&bill, data, sizeof(bill));

                // Process the bill
                return processBill(bill);
            }

        } // namespace network
    } // namespace infrastructure
} // namespace finance