#pragma once

#include "domain/IPackageHandler.hpp"
#include "domain/FinanceDataStructure.hpp"
#include "domain/Result.hpp"
#include "domain/IFinanceRepository.hpp"
#include "utils/FinanceUtils.hpp"
#include "infrastructure/config/AreaBranchProvider.hpp"
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

        Result<void, ErrorResult> handle(const FinancePackageMessage &pkg) override
        {
            LOG_F(INFO, "Hcrtm01Handler::handle");

            const auto &hcrtm01 = pkg.ap_data.data.hcrtm01;

            // Extract area_center
            std::string headerAreaCenter = FinanceUtils::trim_right(pkg.ap_data.system, sizeof(pkg.ap_data.system));
            std::string dataAreaCenter = FinanceUtils::trim_right(hcrtm01.area_center, sizeof(hcrtm01.area_center));
            // Validate area centers match
            if (headerAreaCenter != dataAreaCenter)
            {
                LOG_F(ERROR, "Hcrtm01Handler::Header area center (%s) does not match data area center (%s)",
                      headerAreaCenter.c_str(), dataAreaCenter.c_str());
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InvalidPacket, "Area center mismatch"});
            }

            if (!config::AreaBranchProvider::IsValidAreaCenter(headerAreaCenter))
            {
                LOG_F(ERROR, "Hcrtm01Handler::Header area center (%s) InValid !", headerAreaCenter.c_str());
                return Result<void, ErrorResult>::Err(ErrorResult{ErrorCode::InvalidPacket, "Area center InValid"});
            }

            // Extract stock_id from ap_data
            std::string stock_id = FinanceUtils::trim_right(hcrtm01.stock_id, sizeof(hcrtm01.stock_id));

            auto get_result = repo_->getData(stock_id);
            if (get_result.is_err())
            {
                LOG_F(ERROR, "Hcrtm01Handler:Failed to get summary data for stock_id=%s", stock_id.c_str());
                return Result<void, ErrorResult>::Err(get_result.unwrap_err());
            }

            struct domain::SummaryData *summary_data = get_result.unwrap();

            // Convert to integers
            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_amount);
            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_buy_order_amount);
            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_sell_match_amount);
            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_qty);
            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_buy_order_qty);
            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_sell_match_qty);
            CONVERT_BACKOFFICE_INT64(hcrtm01, short_amount);
            CONVERT_BACKOFFICE_INT64(hcrtm01, short_sell_order_amount);
            CONVERT_BACKOFFICE_INT64(hcrtm01, short_qty);
            CONVERT_BACKOFFICE_INT64(hcrtm01, short_sell_order_qty);
            CONVERT_BACKOFFICE_INT64(hcrtm01, short_after_hour_sell_order_amount);
            CONVERT_BACKOFFICE_INT64(hcrtm01, short_after_hour_sell_order_qty);
            CONVERT_BACKOFFICE_INT64(hcrtm01, short_sell_match_amount);
            CONVERT_BACKOFFICE_INT64(hcrtm01, short_sell_match_qty);
            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_after_hour_buy_order_amount);
            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_after_hour_buy_order_qty);
            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_buy_match_amount);
            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_buy_match_qty);

            // 處理後資料
            int64_t after_margin_available_amount = margin_amount - margin_buy_match_amount + margin_sell_match_amount - margin_after_hour_buy_order_amount;
            int64_t after_margin_available_qty = margin_qty - margin_buy_match_qty + margin_sell_match_qty - margin_after_hour_buy_order_qty;
            int64_t after_short_available_amount = short_amount - short_sell_match_amount - short_after_hour_sell_order_amount;
            int64_t after_short_available_qty = short_qty - short_sell_order_qty - short_after_hour_sell_order_qty;
            int64_t margin_available_amount = margin_amount - margin_buy_order_amount + margin_sell_match_amount;
            int64_t margin_available_qty = margin_qty - margin_buy_order_qty + margin_sell_match_qty;
            int64_t short_available_amount = short_amount - short_sell_order_amount;
            int64_t short_available_qty = short_qty - short_sell_order_qty;

            // Calculate available amounts and quantities
            summary_data->stock_id = stock_id;
            summary_data->area_center = dataAreaCenter;

            // DEBUG: 加上 資買互抵 temp 值
            const int64_t buy_offset = summary_data->margin_buy_offset_qty;
            const int64_t sell_offset = summary_data->short_sell_offset_qty;
            summary_data->margin_available_qty = margin_available_qty + buy_offset;
            summary_data->after_margin_available_qty = after_margin_available_qty + buy_offset;
            summary_data->short_available_qty = short_available_qty + sell_offset;
            summary_data->after_short_available_qty = after_short_available_qty + sell_offset;

            // 原先的內容
            summary_data->margin_available_amount = margin_available_amount;
            summary_data->short_available_amount = short_available_amount;
            summary_data->after_margin_available_amount = after_margin_available_amount;
            summary_data->after_short_available_amount = after_short_available_amount;
            // Add branch information
            summary_data->belong_branches = config::AreaBranchProvider::getBranchesForArea(dataAreaCenter);

            auto sync_result = repo_->sync(stock_id, summary_data);

            if (sync_result.is_err())
            {
                // If sync failed, execute the error handling logic originally in map_err
                const auto &e = sync_result.unwrap_err();
                LOG_F(ERROR, "Hcrtm01Handler::Failed to sync data for stock_id=%s, error=%s", summary_data->stock_id.c_str(), e.message.c_str());
                // The handle function needs to return Result<void, ErrorResult>.
                // Return an Err of that type, wrapping the original error message.
                return Result<void, ErrorResult>::Err(ErrorResult{e.code, "Hcrtm01Handler::Redis sync error : " + e.message});
            }

            auto update_result = repo_->update(stock_id);

            if (update_result.is_err())
            {
                // If updateCompanySummary failed, handle the error
                const auto &e = update_result.unwrap_err();
                // It's good practice to log the specific failure step
                LOG_F(ERROR, "Hcrtm01Handler::Failed to update company summary for stock_id=%s, error=%s", summary_data->stock_id.c_str(), e.message.c_str());
                // Return an Err of type Result<void, ErrorResult>, wrapping the error.
                return Result<void, ErrorResult>::Err(ErrorResult{e.code, "Hcrtm01Handler::Update Company Summary error : " + e.message}); // More specific error message
            }

            return Result<void, ErrorResult>::Ok();
        }

    private:
        std::shared_ptr<IFinanceRepository<SummaryData, ErrorResult>> repo_;
    };
} // namespace finance::infrastructure::network