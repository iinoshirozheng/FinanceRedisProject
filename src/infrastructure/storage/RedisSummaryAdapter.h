#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "../../domain/IFinanceRepository.h"
#include "../../domain/FinanceDataStructures.h"
#include <hiredis/hiredis.h>

// Forward declare redisContext from hiredis
struct redisContext;
struct redisReply;

namespace finance
{
    namespace infrastructure
    {
        namespace storage
        {
            class RedisSummaryAdapter : public domain::IFinanceRepository<domain::SummaryData>
            {
            public:
                RedisSummaryAdapter(const std::string &redis_url);
                ~RedisSummaryAdapter();

                /* Implementing IFinanceRepository methods */
                bool save(const domain::SummaryData &data) override;
                domain::SummaryData *get(const std::string &key) override;
                bool update(const domain::SummaryData &data, const std::string &key) override;
                bool remove(const std::string &key) override;
                std::vector<domain::SummaryData> loadAll() override;
                std::map<std::string, domain::SummaryData> getAllMapped() override;
                std::vector<domain::SummaryData> getAllBySecondaryKey(const std::string &secondaryKey) override;
                bool createIndex() override;

            private:
                domain::ConfigData config_;
                // Redis connection context
                redisContext *context;
            };

        } // namespace storage

    } // namespace infrastructure

} // namespace finance
