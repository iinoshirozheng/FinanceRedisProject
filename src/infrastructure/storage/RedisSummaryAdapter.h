#pragma once

#include "../../domain/FinanceDataStructure.h"
#include "../../domain/IFinanceRepository.h"
#include <hiredis/hiredis.h>
#include <memory>
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <mutex>

namespace finance::infrastructure::storage
{

    class RedisSummaryAdapter : public domain::IFinanceRepository<domain::SummaryData>
    {
    public:
        explicit RedisSummaryAdapter(const std::string &redis_url);
        ~RedisSummaryAdapter() override;

        bool save(const domain::SummaryData &data) override;
        domain::SummaryData *get(const std::string &key) override;
        bool update(const std::string &key, const domain::SummaryData &data) override;
        bool remove(const std::string &key) override;
        std::vector<domain::SummaryData> loadAll() override;
        std::map<std::string, domain::SummaryData> getAllMapped() override;
        std::vector<domain::SummaryData> getAllBySecondaryKey(const std::string &secondaryKey) override;
        bool createIndex() override;
        bool updateCompanySummary(const std::string &stock_id);

    private:
        redisContext *context_ptr_;
        std::string redisUrl_;
        std::mutex mapLock_;
    };

} // namespace finance::infrastructure::storage
