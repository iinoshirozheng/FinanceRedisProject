#pragma once
#include <unordered_map>
#include <memory>
#include "../../domain/FinanceDataStructures.h"
#include "../../domain/IPacketHandler.h"
#include <iostream>
#include <string_view>

namespace finance
{
    namespace infrastructure
    {
        namespace network
        {
            // HCRTM01 封包的處理器
            class Hcrtm01Handler : public domain::IPackageHandler
            {
            public:
                explicit Hcrtm01Handler(domain::MessageDataHCRTM01 data) : data_(data) {}
                bool processData(domain::ApData &ap_data) override;

            private:
                domain::MessageDataHCRTM01 data_;
            };

            // HCRTM05P 封包的處理器
            class Hcrtm05pHandler : public domain::IPackageHandler
            {
            public:
                explicit Hcrtm05pHandler(domain::MessageDataHCRTM05P data) : data_(data) {}
                bool processData(domain::ApData &ap_data) override;

            private:
                domain::MessageDataHCRTM05P data_;
            };

            class PacketProcessorFactory
            {
            public:
                PacketProcessorFactory();
                ~PacketProcessorFactory();

                PacketProcessorFactory(const PacketProcessorFactory &) = delete;
                PacketProcessorFactory &operator=(const PacketProcessorFactory &) = delete;
                bool ProcessPackage(domain::FinancePackageMessage &message);

            private:
                domain::IPackageHandler *getProcessorHandler(const std::string_view &tcode);

                std::unique_ptr<Hcrtm01Handler> Hcrtm01_handle_;
                std::unique_ptr<Hcrtm05pHandler> Hcrtm05p_handle_;
                // TODO: add finance service
            };

        } // namespace network

    } // namespace infrastructure

} // namespace finance