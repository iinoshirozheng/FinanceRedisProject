#include "RedisSummaryAdapter.h"
#include <hiredis/hiredis.h>
#include <sstream>
#include <iostream>

namespace finance
{
    namespace infrastructure
    {
        namespace storage
        {

            RedisSummaryAdapter::RedisSummaryAdapter(const std::string &host, int port)
                : context(nullptr)
            {
                // Connect to Redis server
                context = redisConnect(host.c_str(), port);
                if (context == nullptr || context->err)
                {
                    if (context)
                    {
                        std::string error = context->errstr;
                        redisFree(context);
                        throw std::runtime_error("Redis connection error: " + error);
                    }
                    else
                    {
                        throw std::runtime_error("Redis connection error: Cannot allocate Redis context");
                    }
                }
            }

            RedisSummaryAdapter::~RedisSummaryAdapter()
            {
                if (context)
                {
                    redisFree(context);
                }
            }

            bool RedisSummaryAdapter::saveSummary(const finance::domain::SummaryData &summary)
            {
                if (!context)
                    return false;

                std::string key = "summary:" + summary.areaCenter;
                std::string value = serializeSummary(summary);

                redisReply *reply = static_cast<redisReply *>(
                    redisCommand(context, "SET %s %s", key.c_str(), value.c_str()));

                bool success = false;
                if (reply)
                {
                    success = (reply->type == REDIS_REPLY_STATUS &&
                               std::string(reply->str) == "OK");
                    freeReplyObject(reply);
                }

                return success;
            }

            std::optional<finance::domain::SummaryData> RedisSummaryAdapter::getSummary(const std::string &areaCenter)
            {
                if (!context)
                    return std::nullopt;

                std::string key = "summary:" + areaCenter;

                redisReply *reply = static_cast<redisReply *>(
                    redisCommand(context, "GET %s", key.c_str()));

                std::optional<finance::domain::SummaryData> result = std::nullopt;
                if (reply)
                {
                    if (reply->type == REDIS_REPLY_STRING)
                    {
                        result = deserializeSummary(reply);
                    }
                    freeReplyObject(reply);
                }

                return result;
            }

            bool RedisSummaryAdapter::updateSummary(const finance::domain::SummaryData &data, const std::string &areaCenter)
            {
                // In Redis, SET will overwrite existing key, so we can just use saveSummary
                return saveSummary(data);
            }

            bool RedisSummaryAdapter::deleteSummary(const std::string &areaCenter)
            {
                if (!context)
                    return false;

                std::string key = "summary:" + areaCenter;

                redisReply *reply = static_cast<redisReply *>(
                    redisCommand(context, "DEL %s", key.c_str()));

                bool success = false;
                if (reply)
                {
                    success = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
                    freeReplyObject(reply);
                }

                return success;
            }

            std::vector<finance::domain::SummaryData> RedisSummaryAdapter::loadAllData()
            {
                std::vector<finance::domain::SummaryData> results;

                if (!context)
                    return results;

                // Get all keys starting with "summary:"
                redisReply *keysReply = static_cast<redisReply *>(
                    redisCommand(context, "KEYS summary:*"));

                if (!keysReply)
                    return results;

                if (keysReply->type == REDIS_REPLY_ARRAY)
                {
                    for (size_t i = 0; i < keysReply->elements; i++)
                    {
                        std::string key = keysReply->element[i]->str;

                        redisReply *dataReply = static_cast<redisReply *>(
                            redisCommand(context, "GET %s", key.c_str()));

                        if (dataReply && dataReply->type == REDIS_REPLY_STRING)
                        {
                            auto summary = deserializeSummary(dataReply);
                            if (summary)
                            {
                                results.push_back(*summary);
                            }
                        }

                        if (dataReply)
                        {
                            freeReplyObject(dataReply);
                        }
                    }
                }

                freeReplyObject(keysReply);
                return results;
            }

            bool RedisSummaryAdapter::createSearchIndex()
            {
                if (!context)
                    return false;

                // This would typically create any Redis search indices necessary
                // For now, just return true as a placeholder
                return true;
            }

            std::string RedisSummaryAdapter::serializeSummary(const finance::domain::SummaryData &summary)
            {
                std::stringstream ss;
                ss << summary.areaCenter << "|"
                   << summary.stockId << "|"
                   << summary.marginBuy << "|"
                   << summary.shortSell << "|"
                   << summary.stockLendingAmount;
                return ss.str();
            }

            std::optional<finance::domain::SummaryData> RedisSummaryAdapter::deserializeSummary(redisReply *reply)
            {
                if (!reply || reply->type != REDIS_REPLY_STRING)
                {
                    return std::nullopt;
                }

                std::string data(reply->str, reply->len);
                std::stringstream ss(data);
                std::string token;

                finance::domain::SummaryData summary;

                // Parse areaCenter
                if (!std::getline(ss, token, '|'))
                    return std::nullopt;
                summary.areaCenter = token;

                // Parse stockId
                if (!std::getline(ss, token, '|'))
                    return std::nullopt;
                summary.stockId = token;

                // Parse marginBuy
                if (!std::getline(ss, token, '|'))
                    return std::nullopt;
                try
                {
                    summary.marginBuy = std::stoll(token);
                }
                catch (const std::exception &e)
                {
                    return std::nullopt;
                }

                // Parse shortSell
                if (!std::getline(ss, token, '|'))
                    return std::nullopt;
                try
                {
                    summary.shortSell = std::stoll(token);
                }
                catch (const std::exception &e)
                {
                    return std::nullopt;
                }

                // Parse stockLendingAmount
                if (!std::getline(ss, token))
                    return std::nullopt;
                try
                {
                    summary.stockLendingAmount = std::stoll(token);
                }
                catch (const std::exception &e)
                {
                    return std::nullopt;
                }

                return summary;
            }

        } // namespace storage
    } // namespace infrastructure
} // namespace finance