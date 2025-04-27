#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
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
            /**
             * @brief Adapter for storing and retrieving summary data with Redis using hiredis
             */
            class RedisSummaryAdapter
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

                /**
                 * @brief Save a summary to Redis
                 * @param summary The summary to save
                 * @return true if successful, false otherwise
                 */
                bool saveSummary(const finance::domain::SummaryData &summary);

                /**
                 * @brief Get a summary by area center
                 * @param areaCenter The area center to look up
                 * @return The summary data if found
                 */
                std::optional<finance::domain::SummaryData> getSummary(const std::string &areaCenter);

                /**
                 * @brief Update an existing summary
                 * @param data The new summary data
                 * @param areaCenter The area center to update
                 * @return true if successful, false otherwise
                 */
                bool updateSummary(const finance::domain::SummaryData &data, const std::string &areaCenter);

                /**
                 * @brief Delete a summary
                 * @param areaCenter The area center to delete
                 * @return true if successful, false otherwise
                 */
                bool deleteSummary(const std::string &areaCenter);

                /**
                 * @brief Load all summary data
                 * @return Vector of all summaries
                 */
                std::vector<finance::domain::SummaryData> loadAllData();

                /**
                 * @brief Create search index for efficient querying
                 * @return true if successful, false otherwise
                 */
                bool createSearchIndex();

            private:
                // Redis connection context
                redisContext *context;

                /**
                 * @brief Serialize summary to string
                 * @param summary The summary to serialize
                 * @return Serialized string representation
                 */
                std::string serializeSummary(const finance::domain::SummaryData &summary);

                /**
                 * @brief Deserialize reply to summary
                 * @param reply The Redis reply containing serialized data
                 * @return The deserialized summary if valid
                 */
                std::optional<finance::domain::SummaryData> deserializeSummary(redisReply *reply);
            };
        } // namespace storage
    } // namespace infrastructure
} // namespace finance