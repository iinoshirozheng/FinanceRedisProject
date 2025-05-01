#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <hiredis/hiredis.h>
#include "../../domain/IFinanceRepository.h"
#include "../../domain/Status.h"
#include "../config/ConnectionConfigProvider.hpp"
#include "../config/AreaBranchProvider.hpp"

namespace finance
{
    namespace infrastructure
    {
        namespace storage
        {
            class RedisSummaryAdapter : public domain::IFinanceRepository<domain::SummaryData>
            {
            public:
                RedisSummaryAdapter(
                    std::shared_ptr<config::ConnectionConfigProvider> configProvider,
                    std::shared_ptr<config::AreaBranchProvider> areaBranchProvider);

                ~RedisSummaryAdapter() override;

                bool save(const domain::SummaryData &data) override;
                domain::SummaryData *get(const std::string &key) override;
                bool remove(const std::string &key) override;
                std::vector<domain::SummaryData> loadAll() override;
                std::map<std::string, domain::SummaryData> getAllMapped() override;
                bool update(const std::string &key, const domain::SummaryData &data) override;
                std::vector<domain::SummaryData> getAllBySecondaryKey(const std::string &secondaryKey) override;
                bool createIndex() override;
                bool updateCompanySummary(const std::string &stock_id);

            private:
                static constexpr const char *KEY_PREFIX = "summary:";
                std::shared_ptr<config::ConnectionConfigProvider> configProvider_;
                std::shared_ptr<config::AreaBranchProvider> areaBranchProvider_;
                redisContext *redisContext_ = nullptr;

                domain::Status connect();
                void disconnect();
                bool find(const std::string &key, domain::SummaryData &data);
                std::string serializeSummaryData(const domain::SummaryData &data) const;
                domain::Status deserializeSummaryData(const std::string &json, domain::SummaryData &data) const;
            };
        } // namespace storage
    } // namespace infrastructure
} // namespace finance
