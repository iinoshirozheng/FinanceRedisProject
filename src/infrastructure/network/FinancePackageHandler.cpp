// infrastructure/network/FinancePackageHandler.cpp
#include "FinancePackageHandler.h"
#include "../config/AreaBranchProvider.hpp"
#include <loguru.hpp>
#include <cstddef> // for offsetof
#include "../../utils/LogHelper.hpp"

namespace finance::infrastructure::network
{
    using domain::ErrorCode;
    using domain::ErrorResult;
    using domain::Result;
    using utils::FinanceUtils;
    namespace config = finance::infrastructure::config;

    PacketProcessorFactory::PacketProcessorFactory()
    {
        handlers_.emplace("ELD001", std::make_unique<Hcrtm01Handler>());
        handlers_.emplace("ELD002", std::make_unique<Hcrtm05pHandler>());
    }

    PacketProcessorFactory::PacketProcessorFactory(std::shared_ptr<storage::RedisSummaryAdapter> repository)
        : repository_(std::move(repository))
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
        char et = ap_data.entry_type[0];
        if (et != 'A' && et != 'C')
            return Result<SummaryData, ErrorResult>::Err(ErrorResult{ErrorCode::InvalidPacket, "Invalid entry type"});

        // 由 ap_data 反推父結構，讀取 t_code
        auto pkg = reinterpret_cast<const domain::FinancePackageMessage *>(
            reinterpret_cast<const char *>(&ap_data) - offsetof(domain::FinancePackageMessage, ap_data));
        std::string_view tcode(pkg->t_code, sizeof(pkg->t_code));

        LOG_F(INFO, "enter processData, t_code=%.*s", int(tcode.size()), tcode.data());

        auto *handler = getProcessorHandler(tcode);
        if (!handler)
        {
            LOG_F(WARNING, "找不到處理器 t_code=%.*s", int(tcode.size()), tcode.data());
            return Result<SummaryData, ErrorResult>::Err(ErrorResult{ErrorCode::UnknownTransactionCode, "Unknown t_code"});
        }

        auto result = static_cast<Hcrtm01Handler *>(handler)->processData(ap_data);
        LOG_F(INFO, "exit processData, result=%s",
              result.is_ok() ? "OK" : result.unwrap_err().message.c_str());
        return result;
    }

    Result<SummaryData>
    Hcrtm01Handler::processData(const domain::ApData &ap_data)
    {
        const auto &h = ap_data.data.hcrtm01;
        // 僅處理符合系統的 area_center
        if (std::string_view(h.area_center, sizeof(h.area_center)) != std::string_view(ap_data.system, sizeof(ap_data.system)))
        {
            return Result<SummaryData, ErrorResult>::Err(ErrorResult{ErrorCode::InvalidPacket, "Invalid area center"});
        }

        // 一次 BCD→int 轉換
        auto ma = FinanceUtils::backOfficeToInt(h.margin_amount, sizeof(h.margin_amount));
        auto mboA = FinanceUtils::backOfficeToInt(h.margin_buy_order_amount, sizeof(h.margin_buy_order_amount));
        auto msmA = FinanceUtils::backOfficeToInt(h.margin_sell_match_amount, sizeof(h.margin_sell_match_amount));
        auto mq = FinanceUtils::backOfficeToInt(h.margin_qty, sizeof(h.margin_qty));
        auto mboQ = FinanceUtils::backOfficeToInt(h.margin_buy_order_qty, sizeof(h.margin_buy_order_qty));
        auto msmQ = FinanceUtils::backOfficeToInt(h.margin_sell_match_qty, sizeof(h.margin_sell_match_qty));
        auto mbmA = FinanceUtils::backOfficeToInt(h.margin_buy_match_amount, sizeof(h.margin_buy_match_amount));
        auto mbmQ = FinanceUtils::backOfficeToInt(h.margin_buy_match_qty, sizeof(h.margin_buy_match_qty));
        auto ahbA = FinanceUtils::backOfficeToInt(h.margin_after_hour_buy_order_amount,
                                                  sizeof(h.margin_after_hour_buy_order_amount));
        auto ahbQ = FinanceUtils::backOfficeToInt(h.margin_after_hour_buy_order_qty,
                                                  sizeof(h.margin_after_hour_buy_order_qty));

        auto sa = FinanceUtils::backOfficeToInt(h.short_amount, sizeof(h.short_amount));
        auto ssoA = FinanceUtils::backOfficeToInt(h.short_sell_order_amount,
                                                  sizeof(h.short_sell_order_amount));
        auto sq = FinanceUtils::backOfficeToInt(h.short_qty, sizeof(h.short_qty));
        auto ssoQ = FinanceUtils::backOfficeToInt(h.short_sell_order_qty,
                                                  sizeof(h.short_sell_order_qty));
        auto ssmA = FinanceUtils::backOfficeToInt(h.short_sell_match_amount,
                                                  sizeof(h.short_sell_match_amount));
        auto ssmQ = FinanceUtils::backOfficeToInt(h.short_sell_match_qty,
                                                  sizeof(h.short_sell_match_qty));
        auto ahsA = FinanceUtils::backOfficeToInt(h.short_after_hour_sell_order_amount,
                                                  sizeof(h.short_after_hour_sell_order_amount));
        auto ahsQ = FinanceUtils::backOfficeToInt(h.short_after_hour_sell_order_qty,
                                                  sizeof(h.short_after_hour_sell_order_qty));

        SummaryData s;
        s.stock_id = std::string_view(h.stock_id, sizeof(h.stock_id));
        s.area_center = std::string_view(h.area_center, sizeof(h.area_center));

        // 盤中可用
        s.margin_available_amount = ma - mboA + msmA;
        s.margin_available_qty = mq - mboQ + msmQ;
        s.short_available_amount = sa - ssoA;
        s.short_available_qty = sq - ssoQ;

        // 盤後可用（Qty 從 match_qty 扣除）
        s.after_margin_available_amount = ma - mbmA + msmA - ahbA;
        s.after_margin_available_qty = mq - mbmQ + msmQ - ahbQ;

        s.after_short_available_amount = sa - ssmA - ahsA;
        s.after_short_available_qty = sq - ssmQ - ahsQ;

        s.belong_branches = config::AreaBranchProvider::getBranchesForArea(s.area_center);

        LOG_CTX(std::string_view(h.stock_id, sizeof(h.stock_id)),
                s.stock_id, s.area_center, INFO,
                "margin_avail_qty=%lld", s.margin_available_qty);

        return Result<SummaryData, ErrorResult>::Ok(std::move(s));
    }

    Result<SummaryData>
    Hcrtm05pHandler::processData(const domain::ApData &ap_data)
    {
        const auto &h = ap_data.data.hcrtm05p;
        SummaryData s;
        s.stock_id = std::string_view(h.stock_id, sizeof(h.stock_id));
        s.area_center = std::string_view(h.broker_id, sizeof(h.broker_id));

        auto buyOff = FinanceUtils::backOfficeToInt(h.margin_buy_offset_qty,
                                                    sizeof(h.margin_buy_offset_qty));
        auto sellOff = FinanceUtils::backOfficeToInt(h.short_sell_offset_qty,
                                                     sizeof(h.short_sell_offset_qty));

        s.margin_available_qty = buyOff;
        s.short_available_qty = sellOff;
        s.after_margin_available_qty = buyOff;
        s.after_short_available_qty = sellOff;
        s.belong_branches = config::AreaBranchProvider::getBranchesForArea(s.area_center);

        LOG_CTX(std::string_view(h.stock_id, sizeof(h.stock_id)),
                s.stock_id, s.area_center, INFO,
                "margin_avail_qty=%lld", s.margin_available_qty);

        return Result<SummaryData, ErrorResult>::Ok(std::move(s));
    }
}
