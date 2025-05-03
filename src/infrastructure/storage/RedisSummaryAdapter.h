#pragma once

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <hiredis/hiredis.h>
#include "../../domain/IFinanceRepository.h"
#include "../../domain/Status.h"
#include "../../domain/FinanceDataStructure.h"
#include "../config/ConnectionConfigProvider.hpp"
#include "../config/AreaBranchProvider.hpp"

namespace finance::infrastructure::storage
{
    class RedisSummaryAdapter : public domain::IFinanceRepository<domain::SummaryData>
    {
    public:
        RedisSummaryAdapter(
            std::shared_ptr<config::ConnectionConfigProvider> configProvider,
            std::shared_ptr<config::AreaBranchProvider> areaBranchProvider);

        ~RedisSummaryAdapter() override;

        // override
        bool setData(const std::string &key, const domain::SummaryData &data) override;
        domain::SummaryData *getData(const std::string &key);
        bool updateData(const std::string &key, const domain::SummaryData &data) override;
        bool removeData(const std::string &key) override;

        // redis method
        std::map<std::string, domain::SummaryData> &getAllMapped();
        bool createRedisTableIndex();
        bool updateCompanySummary(const std::string &stock_id);
        domain::Status connectToRedis();
        void disconnectToRedis();

        domain::Status loadAllFromRedis();
        domain::Status serializeSummaryData(const domain::SummaryData &data, std::string &out_dump);
        domain::Status deserializeSummaryData(const std::string &json, domain::SummaryData &out_data) const;
        domain::Status getSummaryDataFromRedis(const std::string &key, domain::SummaryData &data);
        domain::Status syncToRedis(const domain::SummaryData &data);
        domain::Status removeSummaryDataFromRedis(const std::string &key);
        domain::Status findSummaryDataFromRedis(const std::string &key, domain::SummaryData &out_data);

    private:
        const std::string KEY_PREFIX = "summary";
        std::shared_ptr<config::ConnectionConfigProvider> configProvider_;
        std::shared_ptr<config::AreaBranchProvider> areaBranchProvider_;
        redisContext *redisContext_ = nullptr;
        std::map<std::string, domain::SummaryData> summaryCache_;
    };
} // namespace finance::infrastructure::storage
