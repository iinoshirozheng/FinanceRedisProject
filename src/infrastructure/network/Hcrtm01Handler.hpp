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
            LOG_F(INFO, "Hcrtm01Handler::handle (preparing async tasks)");

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

            // 將從 ELD001 解析出的所有相關數值存入 SummaryData 的 h01_* 欄位
            summary_data->stock_id = stock_id;
            summary_data->area_center = dataAreaCenter;                                                      // 確保 area_center 被設置
            summary_data->belong_branches = config::AreaBranchProvider::getBranchesFromArea(dataAreaCenter); // 更新分支資訊

            // 轉換並儲存所有 HCRTM01 的數值到 SummaryData 的 h01_* 欄位
            // 使用 CONVERT_BACKOFFICE_INT64 宏來處理轉換和錯誤檢查
            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_amount); // 例如這樣轉換第一個欄位
            summary_data->h01_margin_amount = margin_amount;  // 儲存轉換後的數值

            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_buy_order_amount);
            summary_data->h01_margin_buy_order_amount = margin_buy_order_amount;

            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_sell_match_amount);
            summary_data->h01_margin_sell_match_amount = margin_sell_match_amount;

            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_qty);
            summary_data->h01_margin_qty = margin_qty;

            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_buy_order_qty);
            summary_data->h01_margin_buy_order_qty = margin_buy_order_qty;

            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_sell_match_qty);
            summary_data->h01_margin_sell_match_qty = margin_sell_match_qty;

            CONVERT_BACKOFFICE_INT64(hcrtm01, short_amount);
            summary_data->h01_short_amount = short_amount;

            CONVERT_BACKOFFICE_INT64(hcrtm01, short_sell_order_amount);
            summary_data->h01_short_sell_order_amount = short_sell_order_amount;

            CONVERT_BACKOFFICE_INT64(hcrtm01, short_qty);
            summary_data->h01_short_qty = short_qty;

            CONVERT_BACKOFFICE_INT64(hcrtm01, short_sell_order_qty);
            summary_data->h01_short_sell_order_qty = short_sell_order_qty;

            CONVERT_BACKOFFICE_INT64(hcrtm01, short_after_hour_sell_order_amount);
            summary_data->h01_short_after_hour_sell_order_amount = short_after_hour_sell_order_amount;

            CONVERT_BACKOFFICE_INT64(hcrtm01, short_after_hour_sell_order_qty);
            summary_data->h01_short_after_hour_sell_order_qty = short_after_hour_sell_order_qty;

            CONVERT_BACKOFFICE_INT64(hcrtm01, short_sell_match_amount);
            summary_data->h01_short_sell_match_amount = short_sell_match_amount;

            CONVERT_BACKOFFICE_INT64(hcrtm01, short_sell_match_qty);
            summary_data->h01_short_sell_match_qty = short_sell_match_qty;

            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_after_hour_buy_order_amount);
            summary_data->h01_margin_after_hour_buy_order_amount = margin_after_hour_buy_order_amount;

            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_after_hour_buy_order_qty);
            summary_data->h01_margin_after_hour_buy_order_qty = margin_after_hour_buy_order_qty;

            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_buy_match_amount);
            summary_data->h01_margin_buy_match_amount = margin_buy_match_amount;

            CONVERT_BACKOFFICE_INT64(hcrtm01, margin_buy_match_qty);
            summary_data->h01_margin_buy_match_qty = margin_buy_match_qty;

            // --- 呼叫 SummaryData 的方法進行計算 ---
            summary_data->calculate_availables();

            // Create a copy of the summary data for async operations
            SummaryData summary_data_copy = *summary_data;

            // Construct the Redis key
            std::string redis_key = "summary:" + summary_data_copy.area_center + ":" + summary_data_copy.stock_id;

            // Submit async tasks
            LOG_F(INFO, "Hcrtm01Handler: Submitting async SYNC task for key: %s", redis_key.c_str());
            auto sync_future = repo_->sync_async(redis_key, summary_data_copy);

            LOG_F(INFO, "Hcrtm01Handler: Submitting async UPDATE task for stock_id: %s", summary_data_copy.stock_id.c_str());
            auto update_future = repo_->update_async(summary_data_copy.stock_id);

            // Log that tasks have been submitted
            LOG_F(INFO, "Hcrtm01Handler: Async tasks for SYNC and UPDATE submitted for stock_id=%s, area_center=%s.",
                  summary_data_copy.stock_id.c_str(), summary_data_copy.area_center.c_str());

            // Return success since tasks have been submitted
            return Result<void, ErrorResult>::Ok();
        }

    private:
        std::shared_ptr<IFinanceRepository<SummaryData, ErrorResult>> repo_;
    };
} // namespace finance::infrastructure::network
