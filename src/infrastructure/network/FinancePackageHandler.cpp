#include "FinancePackageHandler.h"
#include "../../common/FinanceUtils.hpp"
#include <loguru.hpp>

namespace finance
{
    namespace infrastructure
    {
        namespace network
        {

#define BACK_OFFICE_INT(_DATA_) common::FinanceUtils::backOfficeToInt(_DATA_, sizeof(_DATA_))
            bool Hcrtm01Handler::processData(domain::ApData &ap_data)
            {
                const domain::MessageDataHCRTM01 &hcrtm01 = ap_data.data.hcrtm01;
                if (common::STR_VIEW(hcrtm01.area_center) == common::STR_VIEW(ap_data.system))
                {
                    // Convert string values to integers
                    int64_t margin_amount = BACK_OFFICE_INT(hcrtm01.margin_amount);
                    int64_t margin_buy_order_amount = BACK_OFFICE_INT(hcrtm01.margin_buy_order_amount);
                    int64_t margin_sell_match_amount = BACK_OFFICE_INT(hcrtm01.margin_sell_match_amount);
                    int64_t margin_qty = BACK_OFFICE_INT(hcrtm01.margin_qty);
                    int64_t margin_buy_order_qty = BACK_OFFICE_INT(hcrtm01.margin_buy_order_qty);
                    int64_t margin_sell_match_qty = BACK_OFFICE_INT(hcrtm01.margin_sell_match_qty);
                    int64_t short_amount = BACK_OFFICE_INT(hcrtm01.short_amount);
                    int64_t short_sell_order_amount = BACK_OFFICE_INT(hcrtm01.short_sell_order_amount);
                    int64_t short_qty = BACK_OFFICE_INT(hcrtm01.short_qty);
                    int64_t short_sell_order_qty = BACK_OFFICE_INT(hcrtm01.short_sell_order_qty);
                    int64_t short_after_hour_sell_order_amount = BACK_OFFICE_INT(hcrtm01.short_after_hour_sell_order_amount);
                    int64_t short_after_hour_sell_order_qty = BACK_OFFICE_INT(hcrtm01.short_after_hour_sell_order_qty);
                    int64_t short_sell_match_amount = BACK_OFFICE_INT(hcrtm01.short_sell_match_amount);
                    int64_t short_sell_match_qty = BACK_OFFICE_INT(hcrtm01.short_sell_match_qty);
                    int64_t margin_after_hour_buy_order_amount = BACK_OFFICE_INT(hcrtm01.margin_after_hour_buy_order_amount);
                    int64_t margin_after_hour_buy_order_qty = BACK_OFFICE_INT(hcrtm01.margin_after_hour_buy_order_qty);
                    int64_t margin_buy_match_amount = BACK_OFFICE_INT(hcrtm01.margin_buy_match_amount);
                    int64_t margin_buy_match_qty = BACK_OFFICE_INT(hcrtm01.margin_buy_match_qty);

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

                    // TODO: Get belong branches from area_branch_map
                    // summary_data.belongBranches = ...

                    LOG_F(INFO, "Processed HCRTM01 data for stock: {}, area: {}",
                          summary_data.stockId, summary_data.areaCenter);
                    return true;
                }
                return false;
            }

            bool Hcrtm05pHandler::processData(domain::ApData &ap_data)
            {
                const domain::MessageDataHCRTM05P &hcrtm05p = ap_data.data.hcrtm05p;

                // Convert string values to integers
                int64_t margin_buy_offset_qty = BACK_OFFICE_INT(hcrtm05p.margin_buy_offset_qty);
                int64_t short_sell_offset_qty = BACK_OFFICE_INT(hcrtm05p.short_sell_offset_qty);

                // Create summary data
                domain::SummaryData summary_data;
                summary_data.stockId = common::STR_VIEW(hcrtm05p.stock_id);
                summary_data.areaCenter = common::STR_VIEW(hcrtm05p.broker_id);
                summary_data.marginAvailableQty = margin_buy_offset_qty;
                summary_data.shortAvailableQty = short_sell_offset_qty;
                summary_data.afterMarginAvailableQty = margin_buy_offset_qty;
                summary_data.afterShortAvailableQty = short_sell_offset_qty;

                LOG_F(INFO, "Processed HCRTM05P data for stock: {}, broker: {}",
                      summary_data.stockId, summary_data.areaCenter);
                return true;
            }

            // class PacketProcessorFactory
            PacketProcessorFactory::PacketProcessorFactory()
            {
                Hcrtm01_handle_ = std::make_unique<Hcrtm01Handler>(domain::MessageDataHCRTM01{});
                Hcrtm05p_handle_ = std::make_unique<Hcrtm05pHandler>(domain::MessageDataHCRTM05P{});
            }

            PacketProcessorFactory::~PacketProcessorFactory()
            {
                Hcrtm01_handle_.release();
                Hcrtm05p_handle_.release();
            }

            bool PacketProcessorFactory::ProcessPackage(domain::FinancePackageMessage &message)
            {
                auto tcode = std::string_view(message.t_code, sizeof(message.t_code));
                // LOG_F(INFO, "packet format tcode:{} enttype:{}", packet_format, ap.enttype[0]);
                if (message.ap_data.entry_type[0] != 'A' && message.ap_data.entry_type[0] != 'C')
                {
                    // LOG_F(INFO, "Error enttype");
                    return false;
                }

                domain::IPackageHandler *handler = getProcessorHandler(tcode);

                if (handler == nullptr)
                {
                    // LOG_F(INFO, "Error IPackageHandler");
                    return false;
                }

                handler->processData(message.ap_data);
                return true;
            }

            domain::IPackageHandler *PacketProcessorFactory::getProcessorHandler(const std::string_view &tcode)
            {
                if (tcode == "ELD001")
                    return Hcrtm01_handle_.get();
                else if (tcode == "ELD002")
                    return Hcrtm05p_handle_.get();
                return nullptr;
            }

        } // namespace network

    } // namespace infrastructure

} // namespace finance
