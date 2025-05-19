
#pragma once

#include "domain/IPackageHandler.hpp"
#include "domain/Result.hpp"
#include "domain/FinanceDataStructure.hpp"
#include "Hcrtm01Handler.hpp"
#include "Hcrtm05pHandler.hpp"
#include "utils/FinanceUtils.hpp"
#include <string>
#include <unordered_map>
#include <memory>
#include <loguru.hpp>
#include <cstring>

namespace finance::infrastructure::network
{
    using finance::domain::ErrorCode;
    using finance::domain::ErrorResult;
    using finance::domain::IPackageHandler;
    using finance::domain::Result;
    using finance::domain::SummaryData;
    using utils::FinanceUtils;

    class TransactionProcessor : public domain::IPackageHandler
    {
    public:
        explicit TransactionProcessor(std::shared_ptr<IFinanceRepository<SummaryData, ErrorResult>> repo)
        {
            registerHandler("ELD001", std::make_unique<infrastructure::network::Hcrtm01Handler>(repo));
            registerHandler("ELD002", std::make_unique<infrastructure::network::Hcrtm05pHandler>(repo));
        }

        // Implement IPackageHandler interface
        Result<void, ErrorResult> handle(const domain::FinancePackageMessage &pkg) override
        {
            // 1) 驗證 entry_type
            char et = pkg.ap_data.entry_type[0];
            if (et != 'A' && et != 'C')
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InvalidPacket, "Invalid entry type"});

            // 2) Properly extract t_code with correct handling of null termination
            std::string tcode = std::string(pkg.t_code, sizeof(pkg.t_code));
            // Alternative using FinanceUtils:
            // std::string tcode = FinanceUtils::trim_right(pkg.t_code, sizeof(pkg.t_code));

            LOG_F(INFO, "Processing message with t_code='%s'", tcode.c_str());

            // 3) 取得對應的處理器
            auto it = handlers_.find(tcode);
            if (it == handlers_.end())
            {
                LOG_F(WARNING, "找不到處理器 t_code='%s'", tcode.c_str());
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::UnknownTransactionCode, "Unknown t_code"});
            }

            // 4) 動態呼叫 handler
            auto result = it->second->handle(pkg);

            // 5) 日誌結果狀態
            LOG_F(INFO, "exit process, result=%s",
                  result.is_ok() ? "OK" : result.unwrap_err().message.c_str());

            return result;
        }

    private:
        // Register a handler for a specific transaction code
        void registerHandler(const std::string &tcode, std::unique_ptr<IPackageHandler> handler)
        {
            handlers_[tcode] = std::move(handler);
            LOG_F(INFO, "Registered handler for t_code '%s'", tcode.c_str());
        }

        std::unordered_map<std::string, std::unique_ptr<IPackageHandler>> handlers_;
    };
}
