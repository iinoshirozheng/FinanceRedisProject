#pragma once

#include "domain/IFinanceRepository.h"
#include "domain/FinanceDataStructure.h"
#include "domain/Status.h"
#include "utils/FinanceUtils.hpp"
#include "infrastructure/config/AreaBranchProvider.hpp"
#include "infrastructure/config/ConnectionConfigProvider.hpp"
#include <hiredis/hiredis.h>
#include <map>
#include <memory>
#include <string>

namespace finance::infrastructure::storage
{
    class RedisSummaryAdapter
    {
    public:
        RedisSummaryAdapter();
        ~RedisSummaryAdapter();
        static bool init();
        static domain::Status connectToRedis();
        static void disconnectToRedis();
        static domain::Status syncToRedis(const domain::SummaryData &data);
        static bool updateCompanySummary(const std::string &stock_id);
        static domain::SummaryData *getData(const std::string &key);
        static bool setData(const std::string &key, const domain::SummaryData &data);
        static bool removeData(const std::string &key);
        static domain::Status loadAllFromRedis();
        static std::map<std::string, domain::SummaryData> &getAllMapped();
        static bool updateData(const std::string &key, const domain::SummaryData &data);

    private:
        static const std::string KEY_PREFIX;
        static redisContext *redisContext_;
        static std::map<std::string, domain::SummaryData> summaryCache_;

        static domain::Status serializeSummaryData(const domain::SummaryData &data, std::string &out_json);
        static domain::Status deserializeSummaryData(const std::string &json, domain::SummaryData &out_data);
        static domain::Status getSummaryDataFromRedis(const std::string &key, domain::SummaryData &data);
        static domain::Status removeSummaryDataFromRedis(const std::string &key);
        static domain::Status findSummaryDataFromRedis(const std::string &key, domain::SummaryData &out_data);
        static bool createRedisTableIndex();
    };
} // namespace finance::infrastructure::storage
