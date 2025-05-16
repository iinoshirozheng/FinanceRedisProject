
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

        Result<void, ErrorResult> handle(const FinancePackageMessage &pkg) override
        {
            LOG_F(INFO, "Hcrtm05pHandler::handle");

            const auto &hcrtm05p = pkg.ap_data.data.hcrtm05p;

            // Extract stock_id and broker_id
            std::string stock_id = FinanceUtils::trim_right(hcrtm05p.stock_id, sizeof(hcrtm05p.stock_id));
            std::string area_center = FinanceUtils::trim_right(hcrtm05p.broker_id, sizeof(hcrtm05p.broker_id));

            // Validate broker_id

            if (!config::AreaBranchProvider::IsValidAreaCenter(area_center))
            {
                LOG_F(ERROR, "Invalid broker_id for stock_id=%s", stock_id.c_str());
                return Result<void, ErrorResult>::Err(ErrorResult{ErrorCode::InvalidPacket, "Invalid broker_id"});
            }

            // Convert integers
            CONVERT_BACKOFFICE_INT64(hcrtm05p, margin_buy_offset_qty);
            CONVERT_BACKOFFICE_INT64(hcrtm05p, short_sell_offset_qty);

            // Try to get existing data to merge any other fields we don't know about
            auto existing = repo_->getData(stock_id);
            if (existing.is_err())
            {
                LOG_F(ERROR, "Failed to get data for stock_id=%s", stock_id.c_str());
                return Result<void, ErrorResult>::Err(existing.unwrap_err());
            }

            domain::SummaryData *summary_data = existing.unwrap();
            if (summary_data == nullptr)
            {
                LOG_F(ERROR, "Hcrtm05pHandler:Unexpact Summary Data null, StockId=%s", stock_id.c_str());
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::UnexpectedError, "Hcrtm05pHandler:summary_data = nullptr"});
            }

            // Create summary data
            summary_data->stock_id = stock_id;
            summary_data->area_center = area_center;
            summary_data->margin_buy_offset_qty = margin_buy_offset_qty;
            summary_data->short_sell_offset_qty = short_sell_offset_qty;

            LOG_F(INFO, "Processed stock_id=%s, broker_id=%s, margin_buy_offset_qty=%lld, short_sell_offset_qty=%lld",
                  stock_id.c_str(), area_center.c_str(), margin_buy_offset_qty, short_sell_offset_qty);

            // Update with new values from the message
            summary_data->stock_id = stock_id;
            summary_data->area_center = area_center;
            summary_data->margin_available_qty += margin_buy_offset_qty;
            summary_data->short_available_qty += short_sell_offset_qty;
            summary_data->after_margin_available_qty += margin_buy_offset_qty;
            summary_data->after_short_available_qty += short_sell_offset_qty;
            // 暫存 資買互抵
            summary_data->margin_buy_offset_qty = margin_buy_offset_qty;
            summary_data->short_sell_offset_qty = short_sell_offset_qty;

            // Use set() to store the modified data
            auto set_result = repo_->sync(stock_id, summary_data);
            if (set_result.is_err())
            {
                LOG_F(ERROR, "Failed to update data for stock_id=%s", stock_id.c_str());
                return Result<void, ErrorResult>::Err(set_result.unwrap_err());
            }
            return Result<void, ErrorResult>::Ok();
        }

    private:
        std::shared_ptr<IFinanceRepository<SummaryData, ErrorResult>> repo_;
    };
} // namespace finance::infrastructure::network