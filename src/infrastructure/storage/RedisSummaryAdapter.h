#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "../../domain/FinanceDataStructures.h"
#include "../../domain/IFinanceRepository.h"
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
            /**
             * @brief Adapter for storing and retrieving summary data with Redis using hiredis
             */
            class RedisSummaryAdapter : public domain::IFinanceRepository<domain::SummaryData>
            {
            public:
                /**
                 * @brief Construct a Redis adapter
                 * @param host The Redis server host
                 * @param port The Redis server port
                 */
                RedisSummaryAdapter(const std::string &host = "127.0.0.1", int port = 6379);

                /**
                 * @brief Destructor to clean up Redis connection
                 */
                ~RedisSummaryAdapter();

                // IFinanceRepository interface implementation
                bool save(const domain::SummaryData &data) override;
                domain::SummaryData *get(const std::string &key) override;
                bool update(const domain::SummaryData &data, const std::string &key) override;
                bool remove(const std::string &key) override;
                std::vector<domain::SummaryData> loadAll() override;
                std::map<std::string, domain::SummaryData> getAllMapped() override;
                std::vector<domain::SummaryData> getAllBySecondaryKey(const std::string &secondaryKey) override;
                bool createIndex() override;

            private:
                // Redis connection context
                redisContext *context;

                /**
                 * @brief Serialize summary to string
                 * @param summary The summary to serialize
                 * @return Serialized string representation
                 */
                std::string serializeSummary(const domain::SummaryData &summary);

                /**
                 * @brief Deserialize reply to summary
                 * @param reply The Redis reply containing serialized data
                 * @return The deserialized summary if valid
                 */
                std::optional<domain::SummaryData> deserializeSummary(redisReply *reply);
            };
        } // namespace storage
    } // namespace infrastructure
} // namespace finance