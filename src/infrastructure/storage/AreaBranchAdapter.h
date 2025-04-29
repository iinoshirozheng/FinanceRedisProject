#pragma once

#include <string>
#include <vector>
#include <memory>
#include "../../domain/IFinanceRepository.h"
#include "../../domain/FinanceDataStructures.h"
#include <hiredis/hiredis.h>

namespace finance
{
    namespace infrastructure
    {
        namespace storage
        {
            class AreaBranchAdapter : public domain::IFinanceRepository<domain::SummaryData>
            {
            public:
                explicit AreaBranchAdapter(const std::string &redis_url);
                ~AreaBranchAdapter();

                /* Implementing IFinanceRepository methods */
                bool save(const domain::SummaryData &data) override;
                domain::SummaryData *get(const std::string &key) override;
                bool update(const domain::SummaryData &data, const std::string &key) override;
                bool remove(const std::string &key) override;
                std::vector<domain::SummaryData> loadAll() override;
                std::map<std::string, domain::SummaryData> getAllMapped() override;
                std::vector<domain::SummaryData> getAllBySecondaryKey(const std::string &secondaryKey) override;
                bool createIndex() override;

                /* Additional methods */
                bool loadFromFile(const std::string &filePath);

            private:
                domain::ConfigData config_;
                redisContext *context;
            };

        } // namespace storage
    } // namespace infrastructure
} // namespace finance