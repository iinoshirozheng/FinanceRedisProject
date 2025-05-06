// infrastructure/network/FinancePackageHandler.h
#pragma once

#include "../../domain/Result.hpp"
#include "../../domain/FinanceDataStructure.h"
#include "../../utils/FinanceUtils.hpp"
#include "../config/AreaBranchProvider.hpp"
#include "../../domain/IPackageHandler.h"
#include "../../domain/Error.hpp"

#include <string_view>
#include <unordered_map>

namespace finance::infrastructure::network
{
    using finance::domain::Error;
    using finance::domain::IPackageHandler;
    using finance::domain::Result;
    using finance::domain::SummaryData;

    class Hcrtm01Handler : public domain::IPackageHandler
    {
    public:
        Result<SummaryData> processData(const domain::ApData &ap_data) override;
    };

    class Hcrtm05pHandler : public domain::IPackageHandler
    {
    public:
        Result<SummaryData> processData(const domain::ApData &ap_data) override;
    };

    class PacketProcessorFactory
    {
    public:
        PacketProcessorFactory();
        IPackageHandler *getProcessorHandler(const std::string_view &tcode) const;
        Result<SummaryData> processData(const domain::ApData &ap_data);

    private:
        std::unordered_map<std::string_view, std::unique_ptr<IPackageHandler>> handlers_;
    };
}
