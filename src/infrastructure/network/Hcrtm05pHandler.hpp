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
#include <future>

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

        Result<void, ErrorResult> handle(const FinancePackageMessage &pkg) override
        {
            LOG_F(INFO, "Hcrtm05pHandler::handle (preparing async tasks)");

            const auto &hcrtm05p = pkg.ap_data.data.hcrtm05p;

            std::string stock_id = FinanceUtils::trim_right(hcrtm05p.stock_id, sizeof(hcrtm05p.stock_id));
            std::string area_center = FinanceUtils::trim_right(hcrtm05p.broker_id, sizeof(hcrtm05p.broker_id));

            if (!config::AreaBranchProvider::IsValidAreaCenter(area_center))
            {
                LOG_F(ERROR, "Invalid area_center (%s) for stock_id=%s", area_center.c_str(), stock_id.c_str());
                return Result<void, ErrorResult>::Err(ErrorResult{ErrorCode::InvalidPacket, "Invalid broker_id (not a valid AreaCenter)"});
            }

            std::string key = "summary:" + area_center + ":" + stock_id;
            auto existing = repo_->getData(key);
            if (existing.is_err())
            {
                LOG_F(ERROR, "Hcrtm05pHandler:Failed to get summary data for stock_id=%s, area_center=%s", stock_id.c_str(), area_center.c_str());
                return Result<void, ErrorResult>::Err(existing.unwrap_err());
            }

            domain::SummaryData *summary_data_ptr = existing.unwrap();
            if (summary_data_ptr == nullptr)
            {
                LOG_F(ERROR, "Hcrtm05pHandler:Unexpact Summary Data null after getData, StockId=%s, AreaCenter=%s", stock_id.c_str(), area_center.c_str());
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::UnexpectedError, "Hcrtm05pHandler:summary_data = nullptr after getData"});
            }

            // Convert and store HCRTM05P data to summary_data_ptr
            CONVERT_BACKOFFICE_INT64(hcrtm05p, margin_buy_offset_qty);
            summary_data_ptr->h05p_margin_buy_offset_qty = margin_buy_offset_qty;

            CONVERT_BACKOFFICE_INT64(hcrtm05p, short_sell_offset_qty);
            summary_data_ptr->h05p_short_sell_offset_qty = short_sell_offset_qty;

            // Ensure basic identification info (if summary_data_ptr is newly created)
            if (summary_data_ptr->stock_id.empty())
                summary_data_ptr->stock_id = stock_id;
            if (summary_data_ptr->area_center.empty())
                summary_data_ptr->area_center = area_center;
            if (summary_data_ptr->belong_branches.empty())
            {
                summary_data_ptr->belong_branches = config::AreaBranchProvider::getBranchesFromArea(area_center);
            }

            LOG_F(INFO, "Processed 05p for stock_id=%s, area_center=%s, margin_buy_offset_qty=%lld, short_sell_offset_qty=%lld",
                  stock_id.c_str(), area_center.c_str(), margin_buy_offset_qty, short_sell_offset_qty);

            // Recalculate all available quantities
            summary_data_ptr->calculate_availables();

            // Create a copy of the summary data for async operations
            SummaryData summary_data_copy = *summary_data_ptr;

            // Submit async tasks
            LOG_F(INFO, "Hcrtm05pHandler: Submitting async SYNC task for key: %s", key.c_str());
            auto sync_future = repo_->sync_async(key, summary_data_copy);

            LOG_F(INFO, "Hcrtm05pHandler: Submitting async UPDATE task for stock_id: %s", stock_id.c_str());
            auto update_future = repo_->update_async(stock_id);

            // Log that tasks have been submitted
            LOG_F(INFO, "Hcrtm05pHandler: Async tasks for SYNC and UPDATE submitted for stock_id=%s, area_center=%s.",
                  stock_id.c_str(), area_center.c_str());

            // Return success since tasks have been submitted
            return Result<void, ErrorResult>::Ok();
        }

    private:
        std::shared_ptr<IFinanceRepository<SummaryData, ErrorResult>> repo_;
    };
} // namespace finance::infrastructure::network
