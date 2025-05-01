#include "FinancePackageHandler.h"
#include "../../common/FinanceUtils.hpp"
#include <loguru.hpp>

namespace finance
{
    namespace infrastructure
    {
        namespace network
        {
            using namespace config;

            bool Hcrtm01Handler::processData(domain::ApData &ap_data)
            {
                const domain::MessageDataHCRTM01 &hcrtm01 = ap_data.data.hcrtm01;
                if (common::STR_VIEW(hcrtm01.area_center) == common::STR_VIEW(ap_data.system))
                {
                    // Convert string values to integers
                    int64_t margin_amount = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_amount, sizeof(hcrtm01.margin_amount));
                    int64_t margin_buy_order_amount = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_buy_order_amount, sizeof(hcrtm01.margin_buy_order_amount));
                    int64_t margin_sell_match_amount = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_sell_match_amount, sizeof(hcrtm01.margin_sell_match_amount));
                    int64_t margin_qty = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_qty, sizeof(hcrtm01.margin_qty));
                    int64_t margin_buy_order_qty = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_buy_order_qty, sizeof(hcrtm01.margin_buy_order_qty));
                    int64_t margin_sell_match_qty = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_sell_match_qty, sizeof(hcrtm01.margin_sell_match_qty));
                    int64_t short_amount = common::FinanceUtils::backOfficeToInt(hcrtm01.short_amount, sizeof(hcrtm01.short_amount));
                    int64_t short_sell_order_amount = common::FinanceUtils::backOfficeToInt(hcrtm01.short_sell_order_amount, sizeof(hcrtm01.short_sell_order_amount));
                    int64_t short_qty = common::FinanceUtils::backOfficeToInt(hcrtm01.short_qty, sizeof(hcrtm01.short_qty));
                    int64_t short_sell_order_qty = common::FinanceUtils::backOfficeToInt(hcrtm01.short_sell_order_qty, sizeof(hcrtm01.short_sell_order_qty));
                    int64_t short_after_hour_sell_order_amount = common::FinanceUtils::backOfficeToInt(hcrtm01.short_after_hour_sell_order_amount, sizeof(hcrtm01.short_after_hour_sell_order_amount));
                    int64_t short_after_hour_sell_order_qty = common::FinanceUtils::backOfficeToInt(hcrtm01.short_after_hour_sell_order_qty, sizeof(hcrtm01.short_after_hour_sell_order_qty));
                    int64_t short_sell_match_amount = common::FinanceUtils::backOfficeToInt(hcrtm01.short_sell_match_amount, sizeof(hcrtm01.short_sell_match_amount));
                    int64_t short_sell_match_qty = common::FinanceUtils::backOfficeToInt(hcrtm01.short_sell_match_qty, sizeof(hcrtm01.short_sell_match_qty));
                    int64_t margin_after_hour_buy_order_amount = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_after_hour_buy_order_amount, sizeof(hcrtm01.margin_after_hour_buy_order_amount));
                    int64_t margin_after_hour_buy_order_qty = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_after_hour_buy_order_qty, sizeof(hcrtm01.margin_after_hour_buy_order_qty));
                    int64_t margin_buy_match_amount = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_buy_match_amount, sizeof(hcrtm01.margin_buy_match_amount));
                    int64_t margin_buy_match_qty = common::FinanceUtils::backOfficeToInt(hcrtm01.margin_buy_match_qty, sizeof(hcrtm01.margin_buy_match_qty));

                    // Calculate available amounts
                    int64_t after_margin_available_amount = margin_amount - margin_buy_match_amount + margin_sell_match_amount - margin_after_hour_buy_order_amount;
                    int64_t after_margin_available_qty = margin_qty - margin_buy_match_qty + margin_sell_match_qty - margin_after_hour_buy_order_qty;
                    int64_t after_short_available_amount = short_amount - short_sell_match_amount - short_after_hour_sell_order_amount;
                    int64_t after_short_available_qty = short_qty - short_sell_order_qty - short_after_hour_sell_order_qty;

                    int64_t margin_available_amount = margin_amount - margin_buy_order_amount + margin_sell_match_amount;
                    int64_t margin_available_qty = margin_qty - margin_buy_order_qty + margin_sell_match_qty;
                    int64_t short_available_amount = short_amount - short_sell_order_amount;
                    int64_t short_available_qty = short_qty - short_sell_order_qty;

                    // Create summary data
                    domain::SummaryData summary_data;
                    summary_data.stockId = common::STR_VIEW(hcrtm01.stock_id);
                    summary_data.areaCenter = common::STR_VIEW(hcrtm01.area_center);
                    summary_data.marginAvailableAmount = margin_available_amount;
                    summary_data.marginAvailableQty = margin_available_qty;
                    summary_data.shortAvailableAmount = short_available_amount;
                    summary_data.shortAvailableQty = short_available_qty;
                    summary_data.afterMarginAvailableAmount = after_margin_available_amount;
                    summary_data.afterMarginAvailableQty = after_margin_available_qty;
                    summary_data.afterShortAvailableAmount = after_short_available_amount;
                    summary_data.afterShortAvailableQty = after_short_available_qty;

                    // Get belong branches from area branch provider
                    summary_data.belongBranches = areaBranchProvider_->getBranchesForArea(summary_data.areaCenter);

                    // Save to repository
                    if (!repository_->save(summary_data))
                    {
                        LOG_F(ERROR, "Failed to save summary data for stock: %s", summary_data.stockId.c_str());
                        return false;
                    }

                    LOG_F(INFO, "Processed HCRTM01 data for stock: %s, area: %s",
                          summary_data.stockId.c_str(), summary_data.areaCenter.c_str());
                    return true;
                }
                return false;
            }

            bool Hcrtm05pHandler::processData(domain::ApData &ap_data)
            {
                const domain::MessageDataHCRTM05P &hcrtm05p = ap_data.data.hcrtm05p;

                // Convert string values to integers
                int64_t margin_buy_offset_qty = common::FinanceUtils::backOfficeToInt(hcrtm05p.margin_buy_offset_qty, sizeof(hcrtm05p.margin_buy_offset_qty));
                int64_t short_sell_offset_qty = common::FinanceUtils::backOfficeToInt(hcrtm05p.short_sell_offset_qty, sizeof(hcrtm05p.short_sell_offset_qty));

                // Create summary data
                domain::SummaryData summary_data;
                summary_data.stockId = common::STR_VIEW(hcrtm05p.stock_id);
                summary_data.areaCenter = common::STR_VIEW(hcrtm05p.broker_id);
                summary_data.marginAvailableQty = margin_buy_offset_qty;
                summary_data.shortAvailableQty = short_sell_offset_qty;
                summary_data.afterMarginAvailableQty = margin_buy_offset_qty;
                summary_data.afterShortAvailableQty = short_sell_offset_qty;

                // Get belong branches from area branch provider
                summary_data.belongBranches = areaBranchProvider_->getBranchesForArea(summary_data.areaCenter);

                // Save to repository
                if (!repository_->save(summary_data))
                {
                    LOG_F(ERROR, "Failed to save summary data for stock: %s", summary_data.stockId.c_str());
                    return false;
                }

                LOG_F(INFO, "Processed HCRTM05P data for stock: %s, broker: %s",
                      summary_data.stockId.c_str(), summary_data.areaCenter.c_str());
                return true;
            }

            PacketProcessorFactory::PacketProcessorFactory(
                std::shared_ptr<domain::IFinanceRepository<domain::SummaryData>> repository,
                std::shared_ptr<config::AreaBranchProvider> areaBranchProvider)
                : repository_(std::move(repository)), areaBranchProvider_(std::move(areaBranchProvider))
            {
                handlers_["ELD001"] = std::make_unique<Hcrtm01Handler>(repository_, areaBranchProvider_);
                handlers_["ELD002"] = std::make_unique<Hcrtm05pHandler>(repository_, areaBranchProvider_);
            }

            bool PacketProcessorFactory::processData(domain::ApData &ap_data)
            {
                if (ap_data.entry_type[0] != 'A' && ap_data.entry_type[0] != 'C')
                {
                    LOG_F(WARNING, "Invalid entry type: %c", ap_data.entry_type[0]);
                    return false;
                }

                auto handler = getProcessorHandler(std::string_view(ap_data.data.hcrtm01.area_center, sizeof(ap_data.data.hcrtm01.area_center)));
                if (!handler)
                {
                    LOG_F(WARNING, "No handler found for area center: %s", std::string(ap_data.data.hcrtm01.area_center).c_str());
                    return false;
                }
                return handler->processData(ap_data);
            }

            domain::IPackageHandler *PacketProcessorFactory::getProcessorHandler(const std::string_view &tcode)
            {
                auto it = handlers_.find(tcode);
                return it != handlers_.end() ? it->second.get() : nullptr;
            }

        } // namespace network
    } // namespace infrastructure
} // namespace finance
