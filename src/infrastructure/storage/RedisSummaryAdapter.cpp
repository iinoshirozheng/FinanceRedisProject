#include "RedisSummaryAdapter.h"
#include <nlohmann/json.hpp>
#include <loguru.hpp>

namespace finance
{
    namespace infrastructure
    {
        namespace storage
        {
            RedisSummaryAdapter::RedisSummaryAdapter(const std::string &redis_url)
            {
                config_.redisUrl = redis_url;
                context = redisConnect(redis_url.c_str(), 6379);
                if (context == nullptr || context->err)
                {
                    if (context)
                    {
                        LOG_F(ERROR, "Redis connection error: {}", context->errstr);
                        redisFree(context);
                        context = nullptr;
                    }
                    else
                    {
                        LOG_F(ERROR, "Redis connection error: can't allocate redis context");
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

                try
                {
                    nlohmann::json j;
                    j["stock_id"] = data.stockId;
                    j["area_center"] = data.areaCenter;
                    j["margin_available_amount"] = data.marginAvailableAmount;
                    j["margin_available_qty"] = data.marginAvailableQty;
                    j["short_available_amount"] = data.shortAvailableAmount;
                    j["short_available_qty"] = data.shortAvailableQty;
                    j["after_margin_available_amount"] = data.afterMarginAvailableAmount;
                    j["after_margin_available_qty"] = data.afterMarginAvailableQty;
                    j["after_short_available_amount"] = data.afterShortAvailableAmount;
                    j["after_short_available_qty"] = data.afterShortAvailableQty;
                    j["belong_branches"] = data.belongBranches;

                    std::string key = "summary:" + data.areaCenter + ":" + data.stockId;
                    std::string json_str = j.dump();

                    redisReply *reply = (redisReply *)redisCommand(context, "JSON.SET %s $ %s",
                                                                   key.c_str(), json_str.c_str());

                    bool success = (reply != nullptr && reply->type != REDIS_REPLY_ERROR);
                    if (reply)
                        freeReplyObject(reply);
                    return success;
                }
                catch (const std::exception &ex)
                {
                    LOG_F(ERROR, "Redis save error: {}", ex.what());
                    return false;
                }
            }

            domain::SummaryData *RedisSummaryAdapter::get(const std::string &key)
            {
                if (!context)
                    return nullptr;

                try
                {
                    redisReply *reply = (redisReply *)redisCommand(context, "JSON.GET %s $", key.c_str());
                    if (!reply || reply->type == REDIS_REPLY_ERROR)
                    {
                        if (reply)
                            freeReplyObject(reply);
                        return nullptr;
                    }

                    nlohmann::json j = nlohmann::json::parse(reply->str);
                    freeReplyObject(reply);

                    auto *data = new domain::SummaryData();
                    data->stockId = j[0]["stock_id"].get<std::string>();
                    data->areaCenter = j[0]["area_center"].get<std::string>();
                    data->marginAvailableAmount = j[0]["margin_available_amount"].get<int64_t>();
                    data->marginAvailableQty = j[0]["margin_available_qty"].get<int64_t>();
                    data->shortAvailableAmount = j[0]["short_available_amount"].get<int64_t>();
                    data->shortAvailableQty = j[0]["short_available_qty"].get<int64_t>();
                    data->afterMarginAvailableAmount = j[0]["after_margin_available_amount"].get<int64_t>();
                    data->afterMarginAvailableQty = j[0]["after_margin_available_qty"].get<int64_t>();
                    data->afterShortAvailableAmount = j[0]["after_short_available_amount"].get<int64_t>();
                    data->afterShortAvailableQty = j[0]["after_short_available_qty"].get<int64_t>();
                    data->belongBranches = j[0]["belong_branches"].get<std::vector<std::string>>();

                    return data;
                }
                catch (const std::exception &ex)
                {
                    LOG_F(ERROR, "Redis get error: {}", ex.what());
                    return nullptr;
                }
            }

            bool RedisSummaryAdapter::update(const domain::SummaryData &data, const std::string &key)
            {
                return save(data); // Redis JSON.SET will update if key exists
            }

            bool RedisSummaryAdapter::remove(const std::string &key)
            {
                if (!context)
                    return false;

                redisReply *reply = (redisReply *)redisCommand(context, "DEL %s", key.c_str());
                bool success = (reply != nullptr && reply->type != REDIS_REPLY_ERROR);
                if (reply)
                    freeReplyObject(reply);
                return success;
            }

            std::vector<domain::SummaryData> RedisSummaryAdapter::loadAll()
            {
                std::vector<domain::SummaryData> result;
                if (!context)
                    return result;

                try
                {
                    redisReply *reply = (redisReply *)redisCommand(context, "KEYS summary:*");
                    if (!reply || reply->type != REDIS_REPLY_ARRAY)
                    {
                        if (reply)
                            freeReplyObject(reply);
                        return result;
                    }

                    for (size_t i = 0; i < reply->elements; i++)
                    {
                        std::string key = reply->element[i]->str;
                        auto *data = get(key);
                        if (data)
                        {
                            result.push_back(*data);
                            delete data;
                        }
                    }
                    freeReplyObject(reply);
                }
                catch (const std::exception &ex)
                {
                    LOG_F(ERROR, "Redis loadAll error: {}", ex.what());
                }
                return result;
            }

            std::map<std::string, domain::SummaryData> RedisSummaryAdapter::getAllMapped()
            {
                std::map<std::string, domain::SummaryData> result;
                if (!context)
                    return result;

                try
                {
                    redisReply *reply = (redisReply *)redisCommand(context, "KEYS summary:*");
                    if (!reply || reply->type != REDIS_REPLY_ARRAY)
                    {
                        if (reply)
                            freeReplyObject(reply);
                        return result;
                    }

                    for (size_t i = 0; i < reply->elements; i++)
                    {
                        std::string key = reply->element[i]->str;
                        auto *data = get(key);
                        if (data)
                        {
                            result[key] = *data;
                            delete data;
                        }
                    }
                    freeReplyObject(reply);
                }
                catch (const std::exception &ex)
                {
                    LOG_F(ERROR, "Redis getAllMapped error: {}", ex.what());
                }
                return result;
            }

            std::vector<domain::SummaryData> RedisSummaryAdapter::getAllBySecondaryKey(const std::string &secondaryKey)
            {
                std::vector<domain::SummaryData> result;
                if (!context)
                    return result;

                try
                {
                    std::string pattern = "summary:*:" + secondaryKey;
                    redisReply *reply = (redisReply *)redisCommand(context, "KEYS %s", pattern.c_str());
                    if (!reply || reply->type != REDIS_REPLY_ARRAY)
                    {
                        if (reply)
                            freeReplyObject(reply);
                        return result;
                    }

                    for (size_t i = 0; i < reply->elements; i++)
                    {
                        std::string key = reply->element[i]->str;
                        auto *data = get(key);
                        if (data)
                        {
                            result.push_back(*data);
                            delete data;
                        }
                    }
                    freeReplyObject(reply);
                }
                catch (const std::exception &ex)
                {
                    LOG_F(ERROR, "Redis getAllBySecondaryKey error: {}", ex.what());
                }
                return result;
            }

            bool RedisSummaryAdapter::createIndex()
            {
                if (!context)
                    return false;

                try
                {
                    redisReply *reply = (redisReply *)redisCommand(context,
                                                                   "FT.CREATE outputIdx ON JSON PREFIX 1 summary: SCHEMA "
                                                                   "$.stock_id AS stock_id TEXT "
                                                                   "$.area_center AS area_center TEXT "
                                                                   "$.belong_branches.* AS branches TAG");

                    bool success = (reply != nullptr && reply->type != REDIS_REPLY_ERROR);
                    if (reply)
                        freeReplyObject(reply);
                    return success;
                }
                catch (const std::exception &ex)
                {
                    LOG_F(ERROR, "Redis createIndex error: {}", ex.what());
                    return false;
                }
            }

        } // namespace storage

    } // namespace finance

} // namespace infrastructure