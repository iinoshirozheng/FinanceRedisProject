#pragma once
#include <unordered_map>
#include <memory>
#include "../../domain/FinanceDataStructure.h"
#include "../../domain/IPackageHandler.h"
#include "../../domain/IFinanceRepository.h"
#include "../../infrastructure/config/AreaBranchProvider.hpp"
#include "../../infrastructure/storage/RedisSummaryAdapter.h"
#include <iostream>
#include <string_view>

namespace finance::infrastructure::network
{
    // HCRTM01 封包的處理器
    class Hcrtm01Handler : public domain::IPackageHandler
    {
    public:
        Hcrtm01Handler() = default;
        bool processData(const domain::ApData &ap_data) override;
    };

    // HCRTM05P 封包的處理器
    class Hcrtm05pHandler : public domain::IPackageHandler
    {
    public:
        Hcrtm05pHandler() = default;
        bool processData(const domain::ApData &ap_data) override;
    };

    class PacketProcessorFactory : public domain::IPackageHandler
    {
    public:
        PacketProcessorFactory();
        PacketProcessorFactory(const PacketProcessorFactory &) = delete;
        PacketProcessorFactory &operator=(const PacketProcessorFactory &) = delete;

        bool processData(const domain::ApData &ap_data) override;

    private:
        domain::IPackageHandler *getProcessorHandler(const std::string_view &tcode);
        void initializeHandlers();

        std::unordered_map<std::string_view, std::unique_ptr<domain::IPackageHandler>> handlers_;
    };

} // namespace finance::infrastructure::network