// infrastructure/network/FinancePackageHandler.cpp
#include "FinancePackageHandler.h"
#include "../config/AreaBranchProvider.hpp"
#include <loguru.hpp>
#include "../../utils/LogHelper.hpp"
#include "../../utils/FinanceUtils.hpp" // 確保引入 FinanceUtils

namespace finance::infrastructure::network
{
    using domain::ErrorCode;
    using domain::ErrorResult;
    using domain::Result;
    using utils::FinanceUtils;
    namespace config = finance::infrastructure::config;

    // 預設建構子與注入 repository 的建構子保持不變
    PacketProcessorFactory::PacketProcessorFactory()
    {
        handlers_.emplace("ELD001", std::make_unique<Hcrtm01Handler>());
        handlers_.emplace("ELD002", std::make_unique<Hcrtm05pHandler>());
    }

    domain::IPackageHandler *PacketProcessorFactory::getProcessorHandler(const std::string_view &tcode) const
    {
        auto it = handlers_.find(tcode);
        return it != handlers_.end() ? it->second.get() : nullptr;
    }

    Result<SummaryData> PacketProcessorFactory::processData(const domain::ApData &ap_data)
    {
        throw std::runtime_error("PacketProcessorFactory::processData not implemented");
    }

    Result<SummaryData> PacketProcessorFactory::processData(const std::string_view &tcode, const domain::ApData &ap_data)
    {
        // 1) 驗證 entry_type
        char et = ap_data.entry_type[0];
        if (et != 'A' && et != 'C')
            return Result<SummaryData, ErrorResult>::Err(
                ErrorResult{ErrorCode::InvalidPacket, "Invalid entry type"});

        // 2) 日誌輸出 t_code
        LOG_F(INFO, "enter processData, t_code=%.*s",
              int(tcode.size()), tcode.data());

        // 3) 取得對應的處理器
        auto *handler = getProcessorHandler(tcode);
        if (!handler)
        {
            LOG_F(WARNING, "找不到處理器 t_code=%.*s",
                  int(tcode.size()), tcode.data());
            return Result<SummaryData, ErrorResult>::Err(
                ErrorResult{ErrorCode::UnknownTransactionCode, "Unknown t_code"});
        }

        // 4) 動態呼叫 handler
        auto result = handler->processData(ap_data);

        // 5) 日誌結果狀態
        LOG_F(INFO, "exit processData, result=%s",
              result.is_ok() ? "OK" : result.unwrap_err().message.c_str());

        return result;
    }

    // ELD001 -> HCRTM01
    Result<SummaryData> Hcrtm01Handler::processData(const domain::ApData &ap_data)
    {
        const auto &h = ap_data.data.hcrtm01;

        // 1) trim header 與 data area_center
        auto headerAreaCenter = FinanceUtils::trim_right(ap_data.system, sizeof(ap_data.system));
        auto dataAreaCenter = FinanceUtils::trim_right(h.area_center, sizeof(h.area_center));
        if (headerAreaCenter != dataAreaCenter)
        {
            return Result<SummaryData, ErrorResult>::Err(
                ErrorResult{ErrorCode::InvalidPacket, "Invalid area center"});
        }

        // 2) BCD → int 轉換
        auto margin_amount = FinanceUtils::backOfficeToInt(h.margin_amount, sizeof(h.margin_amount));
        auto margin_buy_order_amount = FinanceUtils::backOfficeToInt(h.margin_buy_order_amount, sizeof(h.margin_buy_order_amount));
        auto margin_sell_match_amount = FinanceUtils::backOfficeToInt(h.margin_sell_match_amount, sizeof(h.margin_sell_match_amount));
        auto margin_qty = FinanceUtils::backOfficeToInt(h.margin_qty, sizeof(h.margin_qty));
        auto margin_buy_order_qty = FinanceUtils::backOfficeToInt(h.margin_buy_order_qty, sizeof(h.margin_buy_order_qty));
        auto margin_sell_match_qty = FinanceUtils::backOfficeToInt(h.margin_sell_match_qty, sizeof(h.margin_sell_match_qty));
        auto short_amount = FinanceUtils::backOfficeToInt(h.short_amount, sizeof(h.short_amount));
        auto short_sell_order_amount = FinanceUtils::backOfficeToInt(h.short_sell_order_amount, sizeof(h.short_sell_order_amount));
        auto short_qty = FinanceUtils::backOfficeToInt(h.short_qty, sizeof(h.short_qty));
        auto short_sell_order_qty = FinanceUtils::backOfficeToInt(h.short_sell_order_qty, sizeof(h.short_sell_order_qty));
        auto short_after_hour_sell_order_amount = FinanceUtils::backOfficeToInt(h.short_after_hour_sell_order_amount, sizeof(h.short_after_hour_sell_order_amount));
        auto short_after_hour_sell_order_qty = FinanceUtils::backOfficeToInt(h.short_after_hour_sell_order_qty, sizeof(h.short_after_hour_sell_order_qty));
        auto short_sell_match_amount = FinanceUtils::backOfficeToInt(h.short_sell_match_amount, sizeof(h.short_sell_match_amount));
        auto short_sell_match_qty = FinanceUtils::backOfficeToInt(h.short_sell_match_qty, sizeof(h.short_sell_match_qty));
        auto margin_after_hour_buy_order_amount = FinanceUtils::backOfficeToInt(h.margin_after_hour_buy_order_amount, sizeof(h.margin_after_hour_buy_order_amount));
        auto margin_after_hour_buy_order_qty = FinanceUtils::backOfficeToInt(h.margin_after_hour_buy_order_qty, sizeof(h.margin_after_hour_buy_order_qty));
        auto margin_buy_match_amount = FinanceUtils::backOfficeToInt(h.margin_buy_match_amount, sizeof(h.margin_buy_match_amount));
        auto margin_buy_match_qty = FinanceUtils::backOfficeToInt(h.margin_buy_match_qty, sizeof(h.margin_buy_match_qty));

        // 3) trim stock_id 與 area_center
        SummaryData s;
        s.stock_id = FinanceUtils::trim_right(h.stock_id, sizeof(h.stock_id));
        s.area_center = headerAreaCenter;

        // 4) 計算「盤中／盤後」可用量
        s.margin_available_amount = margin_amount - margin_buy_order_amount + margin_sell_match_amount;
        s.margin_available_qty = margin_qty - margin_buy_order_qty + margin_sell_match_qty;
        s.short_available_amount = short_amount - short_sell_order_amount;
        s.short_available_qty = short_qty - short_sell_order_qty;

        s.after_margin_available_amount = margin_amount - margin_buy_match_amount + margin_sell_match_amount - margin_after_hour_buy_order_amount;
        s.after_margin_available_qty = margin_qty - margin_buy_match_qty + margin_sell_match_qty - margin_after_hour_buy_order_qty;
        s.after_short_available_amount = short_amount - short_sell_match_amount - short_after_hour_sell_order_amount;
        s.after_short_available_qty = short_qty - short_sell_match_qty - short_after_hour_sell_order_qty;

        // 5) 屬於分支
        s.belong_branches = config::AreaBranchProvider::getBranchesForArea(s.area_center);

        LOG_CTX(std::string_view(h.stock_id, sizeof(h.stock_id)),
                s.stock_id, s.area_center, INFO,
                "margin_avail_qty=%lld", s.margin_available_qty);
        return Result<SummaryData, ErrorResult>::Ok(std::move(s));
    }

    // ELD002 -> HCRTM05P
    Result<SummaryData> Hcrtm05pHandler::processData(const domain::ApData &ap_data)
    {
        const auto &h = ap_data.data.hcrtm05p;

        // 1) trim stock_id 與 broker_id
        std::string stockId = FinanceUtils::trim_right(h.stock_id, sizeof(h.stock_id));
        std::string branchId = FinanceUtils::trim_right(h.broker_id, sizeof(h.broker_id));

        // 2) BCD → int offset
        int64_t buyOff = FinanceUtils::backOfficeToInt(h.margin_buy_offset_qty, sizeof(h.margin_buy_offset_qty));
        int64_t sellOff = FinanceUtils::backOfficeToInt(h.short_sell_offset_qty, sizeof(h.short_sell_offset_qty));

        // 3) 填入 SummaryData
        SummaryData s;
        s.stock_id = stockId;
        s.area_center = branchId;
        s.margin_available_qty = buyOff;
        s.after_margin_available_qty = buyOff;
        s.short_available_qty = sellOff;
        s.after_short_available_qty = sellOff;

        // 4) 屬於分支
        s.belong_branches = config::AreaBranchProvider::getBranchesForArea(s.area_center);

        LOG_CTX(std::string_view(h.stock_id, sizeof(h.stock_id)),
                s.stock_id, s.area_center, INFO,
                "margin_avail_qty=%lld", s.margin_available_qty);
        return Result<SummaryData, ErrorResult>::Ok(std::move(s));
    }

} // namespace finance::infrastructure::network
