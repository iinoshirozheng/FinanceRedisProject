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

    class Hcrtm05pHandler : public domain::IPackageHandler
    {
    public:
        explicit Hcrtm05pHandler(std::shared_ptr<IFinanceRepository<SummaryData, ErrorResult>> repo)
            : repo_(std::move(repo)) {}

        // src/infrastructure/network/Hcrtm05pHandler.hpp 中的 handle 方法
        Result<void, ErrorResult> handle(const FinancePackageMessage &pkg) override
        {
            LOG_F(INFO, "Hcrtm05pHandler::handle");

            const auto &hcrtm05p = pkg.ap_data.data.hcrtm05p;

            // Extract stock_id and area_center (broker_id in 05p maps to area_center in SummaryData?)
            // Need to confirm how broker_id in 05p relates to area_center key for SummaryData
            // Assuming it maps to the area_center that this branch belongs to.
            // Using config::AreaBranchProvider::IsValidAreaCenter(area_center) for validation
            std::string stock_id = FinanceUtils::trim_right(hcrtm05p.stock_id, sizeof(hcrtm05p.stock_id));
            std::string area_center = FinanceUtils::trim_right(hcrtm05p.broker_id, sizeof(hcrtm05p.broker_id));

            // Validate area_center
            if (!config::AreaBranchProvider::IsValidAreaCenter(area_center)) // 驗證 broker_id 是否是有效的 AreaCenter
            {
                LOG_F(ERROR, "Invalid area_center (%s) for stock_id=%s", area_center.c_str(), stock_id.c_str());
                return Result<void, ErrorResult>::Err(ErrorResult{ErrorCode::InvalidPacket, "Invalid broker_id (not a valid AreaCenter)"});
            }

            // 根據 area_center 和 stock_id 獲取 SummaryData 的指標
            // 注意 Key 的格式要與 Hcrtm01Handler 一致： "summary:area_center:stock_id"
            std::string key = "summary:" + area_center + ":" + stock_id;
            auto existing = repo_->getData(key); // 使用正確格式的 Key
            if (existing.is_err())
            {
                LOG_F(ERROR, "Hcrtm05pHandler:Failed to get summary data for stock_id=%s, area_center=%s", stock_id.c_str(), area_center.c_str());
                return Result<void, ErrorResult>::Err(existing.unwrap_err());
            }

            domain::SummaryData *summary_data = existing.unwrap();
            if (summary_data == nullptr)
            {
                LOG_F(ERROR, "Hcrtm05pHandler:Unexpact Summary Data null after getData, StockId=%s, AreaCenter=%s", stock_id.c_str(), area_center.c_str());
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::UnexpectedError, "Hcrtm05pHandler:summary_data = nullptr after getData"});
            }

            // 轉換並儲存 HCRTM05P 的資買互抵數量到 SummaryData 的 h05p_* 欄位
            CONVERT_BACKOFFICE_INT64(hcrtm05p, margin_buy_offset_qty);
            summary_data->h05p_margin_buy_offset_qty = margin_buy_offset_qty;

            // 注意：hcrtm05p.short_sell_offset_qty 在 FinanceDataStructure.hpp 中是 char[6]
            // 轉換為 int64_t
            CONVERT_BACKOFFICE_INT64(hcrtm05p, short_sell_offset_qty);
            summary_data->h05p_short_sell_offset_qty = short_sell_offset_qty;

            // 確保基本的識別資訊也設置正確（即使是 05 封包）
            summary_data->stock_id = stock_id;
            summary_data->area_center = area_center; // 使用根據 broker_id 確定的 area_center
            // belong_branches 通常應該由 01 封包設定，但也可以在這裡確保其存在
            // 如果 SummaryData 是新創建的，可能需要在這裡根據 area_center 設置 belong_branches
            if (summary_data->belong_branches.empty())
            {
                summary_data->belong_branches = config::AreaBranchProvider::getBranchesForArea(area_center);
            }

            LOG_F(INFO, "Processed 05p for stock_id=%s, area_center=%s, margin_buy_offset_qty=%lld, short_sell_offset_qty=%lld",
                  stock_id.c_str(), area_center.c_str(), margin_buy_offset_qty, short_sell_offset_qty);

            // --- 呼叫 SummaryData 的方法重新計算所有可用數量 ---
            summary_data->calculate_availables();

            // 使用 sync() 將更新後的 SummaryData (包含最新的 h01_*, h05p_* 和計算結果) 同步到 Repository
            // 使用正確格式的 Key
            auto set_result = repo_->sync(key, summary_data);
            if (set_result.is_err())
            {
                LOG_F(ERROR, "Hcrtm05pHandler:Failed to sync data for key=%s", key.c_str());
                return Result<void, ErrorResult>::Err(set_result.unwrap_err());
            }

            // ELD002 封包是否需要觸發總公司資料更新？
            // 如果總公司資料是各分區資料的簡單加總，那麼任何分區資料更新後都應該觸發總公司更新。
            // 保留 update 邏輯
            auto update_result = repo_->update(stock_id);

            if (update_result.is_err())
            {
                // ... (錯誤處理與原程式碼相同) ...
                const auto &e = update_result.unwrap_err();
                LOG_F(ERROR, "Hcrtm05pHandler::Failed to update company summary for stock_id=%s, error=%s", summary_data->stock_id.c_str(), e.message.c_str());
                return Result<void, ErrorResult>::Err(ErrorResult{e.code, "Hcrtm05pHandler::Update Company Summary error : " + e.message});
            }

            return Result<void, ErrorResult>::Ok();
        }

    private:
        std::shared_ptr<IFinanceRepository<SummaryData, ErrorResult>> repo_;
    };
} // namespace finance::infrastructure::network