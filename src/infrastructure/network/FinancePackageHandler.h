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
        explicit Hcrtm01Handler(
            std::shared_ptr<finance::infrastructure::storage::RedisSummaryAdapter> repository,
            std::shared_ptr<config::AreaBranchProvider> areaBranchProvider)
            : repository_(repository), areaBranchProvider_(areaBranchProvider) {}

        bool processData(const domain::ApData &ap_data) override;

    private:
        std::shared_ptr<storage::RedisSummaryAdapter> repository_;
        std::shared_ptr<config::AreaBranchProvider> areaBranchProvider_;
    };

    // HCRTM05P 封包的處理器
    class Hcrtm05pHandler : public domain::IPackageHandler
    {
    public:
        explicit Hcrtm05pHandler(
            std::shared_ptr<storage::RedisSummaryAdapter> repository,
            std::shared_ptr<config::AreaBranchProvider> areaBranchProvider)
            : repository_(repository), areaBranchProvider_(areaBranchProvider) {}

        bool processData(const domain::ApData &ap_data) override;

    private:
        std::shared_ptr<storage::RedisSummaryAdapter> repository_;
        std::shared_ptr<config::AreaBranchProvider> areaBranchProvider_;
    };

    class PacketProcessorFactory : public domain::IPackageHandler
    {
    public:
        PacketProcessorFactory(
            std::shared_ptr<storage::RedisSummaryAdapter> repository,
            std::shared_ptr<config::AreaBranchProvider> areaBranchProvider);

        PacketProcessorFactory(const PacketProcessorFactory &) = delete;
        PacketProcessorFactory &operator=(const PacketProcessorFactory &) = delete;

        bool processData(const domain::ApData &ap_data) override;

    private:
        domain::IPackageHandler *getProcessorHandler(const std::string_view &tcode);

        std::unordered_map<std::string_view, std::unique_ptr<domain::IPackageHandler>> handlers_;
        std::shared_ptr<storage::RedisSummaryAdapter> repository_;
        std::shared_ptr<config::AreaBranchProvider> areaBranchProvider_;
    };

} // namespace finance::infrastructure::network