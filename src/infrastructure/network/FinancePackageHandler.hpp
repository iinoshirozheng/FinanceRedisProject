#pragma once

#include "../../domain/IPackageHandler.hpp"
#include "../../domain/FinanceDataStructure.hpp"
#include "../../domain/Result.hpp"
#include "../../domain/IFinanceRepository.hpp"
#include "../../utils/LogHelper.hpp"
#include "../../utils/FinanceUtils.hpp"
#include "../config/AreaBranchProvider.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <loguru.hpp>

namespace finance::infrastructure::network
{
    using domain::ErrorCode;
    using domain::ErrorResult;
    using domain::FinancePackageMessage;
    using domain::IFinanceRepository;
    using domain::Result;
    using domain::SummaryData;
    using utils::FinanceUtils;
    namespace config = finance::infrastructure::config;

    class Hcrtm01Handler : public domain::IPackageHandler
    {
    public:
        explicit Hcrtm01Handler(std::shared_ptr<IFinanceRepository<SummaryData, ErrorResult>> repo)
            : repo_(std::move(repo)) {}

        Result<SummaryData, ErrorResult> handle(const FinancePackageMessage &pkg) override
        {
            LOG_F(INFO, "Hcrtm01Handler::handle");

            const auto &h = pkg.ap_data.data.hcrtm01;

            // Extract stock_id from ap_data
            std::string stock_id = FinanceUtils::trim_right(h.stock_id, sizeof(h.stock_id));

            // Extract area_center
            std::string headerAreaCenter = FinanceUtils::trim_right(pkg.ap_data.system, sizeof(pkg.ap_data.system));
            std::string dataAreaCenter = FinanceUtils::trim_right(h.area_center, sizeof(h.area_center));

            // Validate area centers match
            if (headerAreaCenter != dataAreaCenter)
            {
                LOG_F(ERROR, "Header area center (%s) does not match data area center (%s)",
                      headerAreaCenter.c_str(), dataAreaCenter.c_str());
                return Result<SummaryData, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InvalidPacket, "Area center mismatch"});
            }

            // Convert BCD to integers
            int64_t margin_amount = FinanceUtils::backOfficeToInt(h.margin_amount, sizeof(h.margin_amount));
            int64_t margin_buy_order_amount = FinanceUtils::backOfficeToInt(h.margin_buy_order_amount, sizeof(h.margin_buy_order_amount));
            int64_t margin_sell_match_amount = FinanceUtils::backOfficeToInt(h.margin_sell_match_amount, sizeof(h.margin_sell_match_amount));
            int64_t margin_qty = FinanceUtils::backOfficeToInt(h.margin_qty, sizeof(h.margin_qty));
            int64_t margin_buy_order_qty = FinanceUtils::backOfficeToInt(h.margin_buy_order_qty, sizeof(h.margin_buy_order_qty));
            int64_t margin_sell_match_qty = FinanceUtils::backOfficeToInt(h.margin_sell_match_qty, sizeof(h.margin_sell_match_qty));
            int64_t short_amount = FinanceUtils::backOfficeToInt(h.short_amount, sizeof(h.short_amount));
            int64_t short_sell_order_amount = FinanceUtils::backOfficeToInt(h.short_sell_order_amount, sizeof(h.short_sell_order_amount));
            int64_t short_qty = FinanceUtils::backOfficeToInt(h.short_qty, sizeof(h.short_qty));
            int64_t short_sell_order_qty = FinanceUtils::backOfficeToInt(h.short_sell_order_qty, sizeof(h.short_sell_order_qty));
            int64_t short_after_hour_sell_order_amount = FinanceUtils::backOfficeToInt(h.short_after_hour_sell_order_amount, sizeof(h.short_after_hour_sell_order_amount));
            int64_t short_after_hour_sell_order_qty = FinanceUtils::backOfficeToInt(h.short_after_hour_sell_order_qty, sizeof(h.short_after_hour_sell_order_qty));
            int64_t short_sell_match_amount = FinanceUtils::backOfficeToInt(h.short_sell_match_amount, sizeof(h.short_sell_match_amount));
            int64_t short_sell_match_qty = FinanceUtils::backOfficeToInt(h.short_sell_match_qty, sizeof(h.short_sell_match_qty));
            int64_t margin_after_hour_buy_order_amount = FinanceUtils::backOfficeToInt(h.margin_after_hour_buy_order_amount, sizeof(h.margin_after_hour_buy_order_amount));
            int64_t margin_after_hour_buy_order_qty = FinanceUtils::backOfficeToInt(h.margin_after_hour_buy_order_qty, sizeof(h.margin_after_hour_buy_order_qty));
            int64_t margin_buy_match_amount = FinanceUtils::backOfficeToInt(h.margin_buy_match_amount, sizeof(h.margin_buy_match_amount));
            int64_t margin_buy_match_qty = FinanceUtils::backOfficeToInt(h.margin_buy_match_qty, sizeof(h.margin_buy_match_qty));

            // Create summary data
            SummaryData summary;
            summary.stock_id = stock_id;
            summary.area_center = headerAreaCenter;

            // Calculate available amounts and quantities
            summary.margin_available_amount = margin_amount - margin_buy_order_amount + margin_sell_match_amount;
            summary.margin_available_qty = margin_qty - margin_buy_order_qty + margin_sell_match_qty;
            summary.short_available_amount = short_amount - short_sell_order_amount;
            summary.short_available_qty = short_qty - short_sell_order_qty;

            summary.after_margin_available_amount = margin_amount - margin_buy_match_amount + margin_sell_match_amount - margin_after_hour_buy_order_amount;
            summary.after_margin_available_qty = margin_qty - margin_buy_match_qty + margin_sell_match_qty - margin_after_hour_buy_order_qty;
            summary.after_short_available_amount = short_amount - short_sell_match_amount - short_after_hour_sell_order_amount;
            summary.after_short_available_qty = short_qty - short_sell_match_qty - short_after_hour_sell_order_qty;

            // Add branch information
            summary.belong_branches = config::AreaBranchProvider::getBranchesForArea(summary.area_center);

            LOG_F(INFO, "Processed stock_id=%s, area_center=%s, margin_avail_qty=%lld",
                  summary.stock_id.c_str(), summary.area_center.c_str(), summary.margin_available_qty);

            // Try to get existing data to merge any other fields we don't know about
            auto existing = repo_->get(summary.stock_id);
            if (existing.is_err())
            {
                LOG_F(ERROR, "Failed to get data for stock_id=%s", summary.stock_id.c_str());
                return Result<SummaryData, ErrorResult>::Err(existing.unwrap_err());
            }

            // Preserve any existing data fields not in the message
            auto data = existing.unwrap();

            // Update with new values from the message
            data.area_center = summary.area_center;
            data.margin_available_amount = summary.margin_available_amount;
            data.margin_available_qty = summary.margin_available_qty;
            data.short_available_amount = summary.short_available_amount;
            data.short_available_qty = summary.short_available_qty;
            data.after_margin_available_amount = summary.after_margin_available_amount;
            data.after_margin_available_qty = summary.after_margin_available_qty;
            data.after_short_available_amount = summary.after_short_available_amount;
            data.after_short_available_qty = summary.after_short_available_qty;
            data.belong_branches = summary.belong_branches;

            // Use set() to store the modified data
            auto set_result = repo_->set(summary.stock_id, data);
            if (!set_result.is_ok())
            {
                LOG_F(ERROR, "Failed to update data for stock_id=%s", summary.stock_id.c_str());
                return Result<SummaryData, ErrorResult>::Err(set_result.unwrap_err());
            }
            return Result<SummaryData, ErrorResult>::Ok(data);
        }

    private:
        std::shared_ptr<IFinanceRepository<SummaryData, ErrorResult>> repo_;
    };

    class Hcrtm05pHandler : public domain::IPackageHandler
    {
    public:
        explicit Hcrtm05pHandler(std::shared_ptr<IFinanceRepository<SummaryData, ErrorResult>> repo)
            : repo_(std::move(repo)) {}

        Result<SummaryData, ErrorResult> handle(const FinancePackageMessage &pkg) override
        {
            LOG_F(INFO, "Hcrtm05pHandler::handle");

            const auto &h = pkg.ap_data.data.hcrtm05p;

            // Extract stock_id and broker_id
            std::string stock_id = FinanceUtils::trim_right(h.stock_id, sizeof(h.stock_id));
            std::string broker_id = FinanceUtils::trim_right(h.broker_id, sizeof(h.broker_id));

            // Validate broker_id
            if (broker_id.empty())
            {
                LOG_F(ERROR, "Invalid broker_id for stock_id=%s", stock_id.c_str());
                return Result<SummaryData, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InvalidPacket, "Invalid broker_id"});
            }

            // Convert BCD to integers
            int64_t margin_buy_offset_qty = FinanceUtils::backOfficeToInt(h.margin_buy_offset_qty, sizeof(h.margin_buy_offset_qty));
            int64_t short_sell_offset_qty = FinanceUtils::backOfficeToInt(h.short_sell_offset_qty, sizeof(h.short_sell_offset_qty));

            // Create summary data
            SummaryData summary;
            summary.stock_id = stock_id;
            summary.area_center = broker_id;

            // Set quantities based on offsets
            summary.margin_available_qty = margin_buy_offset_qty;
            summary.after_margin_available_qty = margin_buy_offset_qty;
            summary.short_available_qty = short_sell_offset_qty;
            summary.after_short_available_qty = short_sell_offset_qty;

            // Add branch information
            summary.belong_branches = config::AreaBranchProvider::getBranchesForArea(summary.area_center);

            LOG_F(INFO, "Processed stock_id=%s, broker_id=%s, margin_buy_offset_qty=%lld, short_sell_offset_qty=%lld",
                  summary.stock_id.c_str(), summary.area_center.c_str(), margin_buy_offset_qty, short_sell_offset_qty);

            // Try to get existing data to merge any other fields we don't know about
            auto existing = repo_->get(summary.stock_id);
            if (existing.is_err())
            {
                LOG_F(ERROR, "Failed to get data for stock_id=%s", summary.stock_id.c_str());
                return Result<SummaryData, ErrorResult>::Err(existing.unwrap_err());
            }

            // Preserve any existing data fields not in the message
            auto data = existing.unwrap();

            // Update with new values from the message
            data.area_center = summary.area_center;
            data.margin_available_qty = summary.margin_available_qty;
            data.after_margin_available_qty = summary.after_margin_available_qty;
            data.short_available_qty = summary.short_available_qty;
            data.after_short_available_qty = summary.after_short_available_qty;
            data.belong_branches = summary.belong_branches;

            // Use set() to store the modified data
            auto set_result = repo_->set(summary.stock_id, data);
            if (!set_result.is_ok())
            {
                LOG_F(ERROR, "Failed to update data for stock_id=%s", summary.stock_id.c_str());
                return Result<SummaryData, ErrorResult>::Err(set_result.unwrap_err());
            }
            return Result<SummaryData, ErrorResult>::Ok(data);
        }

    private:
        std::shared_ptr<IFinanceRepository<SummaryData, ErrorResult>> repo_;
    };
} // namespace finance::infrastructure::network