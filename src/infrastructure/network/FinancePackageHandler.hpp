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
    /**
     * @brief FinancePackageHandler 負責處理從 TCP 接收的 FinancePackageMessage，
     *        根據 message 內容更新 SummaryData 並透過 IFinanceRepository 儲存
     */
    class FinancePackageHandler : public domain::IPackageHandler
    {
    public:
        /**
         * @brief 建構子，注入 repository
         * @param repo 資料儲存庫
         */
        explicit FinancePackageHandler(std::shared_ptr<domain::IFinanceRepository<SummaryData, ErrorResult>> repo)
            : repo_(std::move(repo))
        {
        }

        ~FinancePackageHandler() override = default;

        /**
         * @brief 處理 FinancePackageMessage，回傳更新後的 SummaryData 或 ErrorResult
         * @param message 待處理的封包資料
         * @return Result<SummaryData, ErrorResult>
         */
        Result<SummaryData, ErrorResult> handle(const FinancePackageMessage &message) override
        {
            // 依照 message 內容計算 key
            std::string key = FinanceUtils::buildKey(message.stock_id, message.broker_id);
            trim_right(key);

            // 從 repository 讀取既有資料
            auto existing = repo_->get(key);
            if (existing.is_err())
            {
                LOG_F(WARNING, "Key '%s' not found, initializing new SummaryData", key.c_str());
                SummaryData initData{};
                initData.stock_id = message.stock_id;
                initData.area_center = AreaBranchProvider::getAreaCenter(message.broker_id);
                existing = Result<SummaryData, ErrorResult>::Ok(initData);
            }

            // 取得 summary 計算邏輯
            SummaryData summary = FinanceUtils::calculateSummary(existing.unwrap(), message);

            // Preserve any existing data fields not in the message
            auto data = existing.unwrap();

            // Update with new values from the message
            data.area_center = summary.area_center;
            data.margin_available_amount = summary.margin_available_amount;
            data.margin_available_qty = summary.margin_available_qty;
            data.after_margin_available_amount = summary.after_margin_available_amount;
            data.after_margin_available_qty = summary.after_margin_available_qty;
            data.short_available_amount = summary.short_available_amount;
            data.short_available_qty = summary.short_available_qty;
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
        std::shared_ptr<domain::IFinanceRepository<SummaryData, ErrorResult>> repo_;
    };
} // namespace finance::infrastructure::network
