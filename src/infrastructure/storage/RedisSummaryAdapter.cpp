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

            bool RedisSummaryAdapter::save(const domain::SummaryData &data)
            {
                if (!context)
                    return false;

                std::string key = "summary:" + data.areaCenter;
                std::string value = serializeSummary(data);

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

            domain::SummaryData *RedisSummaryAdapter::get(const std::string &key)
            {
                if (!context)
                    return nullptr;

                redisReply *reply = static_cast<redisReply *>(
                    redisCommand(context, "GET %s", key.c_str()));

                domain::SummaryData *result = nullptr;
                if (reply)
                {
                    if (reply->type == REDIS_REPLY_STRING)
                    {
                        auto summary = deserializeSummary(reply);
                        if (summary)
                        {
                            result = new domain::SummaryData(*summary);
                        }
                    }
                    freeReplyObject(reply);
                }

                return result;
            }

            bool RedisSummaryAdapter::update(const domain::SummaryData &data, const std::string &key)
            {
                // In Redis, SET will overwrite existing key, so we can just use save
                return save(data);
            }

            bool RedisSummaryAdapter::remove(const std::string &key)
            {
                if (!context)
                    return false;

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

            std::vector<domain::SummaryData> RedisSummaryAdapter::loadAll()
            {
                std::vector<domain::SummaryData> results;

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

            std::map<std::string, domain::SummaryData> RedisSummaryAdapter::getAllMapped()
            {
                std::map<std::string, domain::SummaryData> results;

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
                                results[key] = *summary;
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

            std::vector<domain::SummaryData> RedisSummaryAdapter::getAllBySecondaryKey(const std::string &secondaryKey)
            {
                std::vector<domain::SummaryData> results;

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
                            if (summary && summary->stockId == secondaryKey)
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

            bool RedisSummaryAdapter::createIndex()
            {
                if (!context)
                    return false;

                // This would typically create any Redis search indices necessary
                // For now, just return true as a placeholder
                return true;
            }

            std::string RedisSummaryAdapter::serializeSummary(const domain::SummaryData &summary)
            {
                std::stringstream ss;
                ss << summary.areaCenter << "|"
                   << summary.stockId << "|"
                   << summary.marginAvailableAmount << "|"
                   << summary.marginAvailableQty << "|"
                   << summary.shortAvailableAmount << "|"
                   << summary.shortAvailableQty << "|"
                   << summary.afterMarginAvailableAmount << "|"
                   << summary.afterMarginAvailableQty << "|"
                   << summary.afterShortAvailableAmount << "|"
                   << summary.afterShortAvailableQty;
                return ss.str();
            }

            std::optional<domain::SummaryData> RedisSummaryAdapter::deserializeSummary(redisReply *reply)
            {
                if (!reply || reply->type != REDIS_REPLY_STRING)
                {
                    return std::nullopt;
                }

                std::string data(reply->str, reply->len);
                std::stringstream ss(data);
                std::string token;

                domain::SummaryData summary;

                // Parse areaCenter
                if (!std::getline(ss, token, '|'))
                    return std::nullopt;
                summary.areaCenter = token;

                // Parse stockId
                if (!std::getline(ss, token, '|'))
                    return std::nullopt;
                summary.stockId = token;

                // Parse marginAvailableAmount
                if (!std::getline(ss, token, '|'))
                    return std::nullopt;
                try
                {
                    summary.marginAvailableAmount = std::stoll(token);
                }
                catch (const std::exception &e)
                {
                    return std::nullopt;
                }

                // Parse marginAvailableQty
                if (!std::getline(ss, token, '|'))
                    return std::nullopt;
                try
                {
                    summary.marginAvailableQty = std::stoll(token);
                }
                catch (const std::exception &e)
                {
                    return std::nullopt;
                }

                // Parse shortAvailableAmount
                if (!std::getline(ss, token, '|'))
                    return std::nullopt;
                try
                {
                    summary.shortAvailableAmount = std::stoll(token);
                }
                catch (const std::exception &e)
                {
                    return std::nullopt;
                }

                // Parse shortAvailableQty
                if (!std::getline(ss, token, '|'))
                    return std::nullopt;
                try
                {
                    summary.shortAvailableQty = std::stoll(token);
                }
                catch (const std::exception &e)
                {
                    return std::nullopt;
                }

                // Parse afterMarginAvailableAmount
                if (!std::getline(ss, token, '|'))
                    return std::nullopt;
                try
                {
                    summary.afterMarginAvailableAmount = std::stoll(token);
                }
                catch (const std::exception &e)
                {
                    return std::nullopt;
                }

                // Parse afterMarginAvailableQty
                if (!std::getline(ss, token, '|'))
                    return std::nullopt;
                try
                {
                    summary.afterMarginAvailableQty = std::stoll(token);
                }
                catch (const std::exception &e)
                {
                    return std::nullopt;
                }

                // Parse afterShortAvailableAmount
                if (!std::getline(ss, token, '|'))
                    return std::nullopt;
                try
                {
                    summary.afterShortAvailableAmount = std::stoll(token);
                }
                catch (const std::exception &e)
                {
                    return std::nullopt;
                }

                // Parse afterShortAvailableQty
                if (!std::getline(ss, token))
                    return std::nullopt;
                try
                {
                    summary.afterShortAvailableQty = std::stoll(token);
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