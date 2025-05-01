#include "RedisSummaryAdapter.h"
#include "../../utils/FinanceUtils.hpp"
#include <loguru.hpp>
#include <nlohmann/json.hpp>
#include <hiredis/hiredis.h>

namespace finance::infrastructure::storage
{
    RedisSummaryAdapter::RedisSummaryAdapter(
        std::shared_ptr<config::ConnectionConfigProvider> configProvider,
        std::shared_ptr<config::AreaBranchProvider> areaBranchProvider)
        : configProvider_(std::move(configProvider)), areaBranchProvider_(std::move(areaBranchProvider))
    {
        auto status = connect();
        if (!status.isOk())
        {
            LOG_F(ERROR, "%s", status.toString().c_str());
        }
    }

    RedisSummaryAdapter::~RedisSummaryAdapter()
    {
        disconnect();
    }

    domain::Status RedisSummaryAdapter::connect()
    {
        try
        {
            if (redisContext_)
            {
                return domain::Status::ok()
                    .withOperation("connect")
                    .withResponse("Already connected");
            }

            redisContext_ = redisConnect(configProvider_->getRedisUrl().c_str(), 6379);
            if (!redisContext_ || redisContext_->err)
            {
                std::string error = redisContext_ ? redisContext_->errstr : "Failed to allocate Redis context";
                return domain::Status::error(domain::Status::Code::ConnectionError, error)
                    .withOperation("connect")
                    .withRequest(configProvider_->getRedisUrl());
            }

            return domain::Status::ok()
                .withOperation("connect")
                .withRequest(configProvider_->getRedisUrl());
        }
        catch (const std::exception &ex)
        {
            return domain::Status::error(domain::Status::Code::ConnectionError, ex.what())
                .withOperation("connect")
                .withRequest(configProvider_->getRedisUrl());
        }
    }

    void RedisSummaryAdapter::disconnect()
    {
        if (redisContext_)
        {
            redisFree(redisContext_);
            redisContext_ = nullptr;
        }
    }

    bool RedisSummaryAdapter::save(const domain::SummaryData &data)
    {
        try
        {
            if (!redisContext_)
            {
                auto status = connect();
                if (!status.isOk())
                {
                    LOG_F(ERROR, "%s", status.toString().c_str());
                    return false;
                }
            }

            std::string key = common::FinanceUtils::generateKey(data);
            std::string json = serializeSummaryData(data);

            redisReply *reply = (redisReply *)redisCommand(redisContext_, "SET %s %s", key.c_str(), json.c_str());
            std::string response = reply ? reply->str : "";

            domain::Status status = reply && reply->type != REDIS_REPLY_ERROR
                                        ? domain::Status::ok()
                                              .withOperation("save")
                                              .withKey(key)
                                              .withRequest(json)
                                              .withResponse(response)
                                        : domain::Status::error(domain::Status::Code::RedisError, reply ? reply->str : "no reply")
                                              .withOperation("save")
                                              .withKey(key)
                                              .withRequest(json)
                                              .withResponse(response);

            if (reply)
                freeReplyObject(reply);

            if (!status.isOk())
            {
                LOG_F(ERROR, "%s", status.toString().c_str());
            }
            else
            {
                LOG_F(INFO, "%s", status.toString().c_str());
            }

            // Update cache
            summaryCache_[key] = data;
            return status.isOk();
        }
        catch (const std::exception &ex)
        {
            auto status = domain::Status::error(domain::Status::Code::RedisError, ex.what())
                              .withOperation("save");
            LOG_F(ERROR, "%s", status.toString().c_str());
            return false;
        }
    }

    bool RedisSummaryAdapter::find(const std::string &key, domain::SummaryData &data)
    {
        try
        {
            if (!redisContext_)
            {
                auto status = connect();
                if (!status.isOk())
                {
                    LOG_F(ERROR, "%s", status.toString().c_str());
                    return false;
                }
            }

            redisReply *reply = (redisReply *)redisCommand(redisContext_, "GET %s", key.c_str());
            std::string response = reply ? reply->str : "";

            domain::Status status;
            if (!reply)
            {
                status = domain::Status::error(domain::Status::Code::RedisError, "No reply")
                             .withOperation("find")
                             .withKey(key)
                             .withResponse(response);
            }
            else if (reply->type == REDIS_REPLY_NIL)
            {
                status = domain::Status::error(domain::Status::Code::NotFound, "Key not found")
                             .withOperation("find")
                             .withKey(key)
                             .withResponse(response);
            }
            else
            {
                status = deserializeSummaryData(reply->str, data);
                status.withOperation("find")
                    .withKey(key)
                    .withResponse(response);
            }

            if (reply)
                freeReplyObject(reply);

            if (!status.isOk())
            {
                LOG_F(ERROR, "%s", status.toString().c_str());
            }
            else
            {
                LOG_F(INFO, "%s", status.toString().c_str());
            }

            return status.isOk();
        }
        catch (const std::exception &ex)
        {
            auto status = domain::Status::error(domain::Status::Code::RedisError, ex.what())
                              .withOperation("find")
                              .withKey(key);
            LOG_F(ERROR, "%s", status.toString().c_str());
            return false;
        }
    }

    bool RedisSummaryAdapter::remove(const std::string &key)
    {
        try
        {
            if (!redisContext_)
            {
                auto status = connect();
                if (!status.isOk())
                {
                    LOG_F(ERROR, "%s", status.toString().c_str());
                    return false;
                }
            }

            redisReply *reply = (redisReply *)redisCommand(redisContext_, "DEL %s", key.c_str());
            std::string response = reply ? reply->str : "";

            domain::Status status = reply && reply->type != REDIS_REPLY_ERROR
                                        ? domain::Status::ok()
                                              .withOperation("remove")
                                              .withKey(key)
                                              .withResponse(response)
                                        : domain::Status::error(domain::Status::Code::RedisError, reply ? reply->str : "no reply")
                                              .withOperation("remove")
                                              .withKey(key)
                                              .withResponse(response);

            if (reply)
                freeReplyObject(reply);

            if (!status.isOk())
            {
                LOG_F(ERROR, "%s", status.toString().c_str());
            }
            else
            {
                LOG_F(INFO, "%s", status.toString().c_str());
            }

            // Remove from cache
            summaryCache_.erase(key);
            return status.isOk();
        }
        catch (const std::exception &ex)
        {
            auto status = domain::Status::error(domain::Status::Code::RedisError, ex.what())
                              .withOperation("remove")
                              .withKey(key);
            LOG_F(ERROR, "%s", status.toString().c_str());
            return false;
        }
    }

    std::vector<domain::SummaryData> RedisSummaryAdapter::loadAll()
    {
        std::vector<domain::SummaryData> result;
        if (!redisContext_)
            return result;

        try
        {
            redisReply *reply = (redisReply *)redisCommand(redisContext_, "KEYS summary:*");
            std::string response = reply ? reply->str : "";

            domain::Status status;
            if (!reply || reply->type != REDIS_REPLY_ARRAY)
            {
                status = domain::Status::error(domain::Status::Code::RedisError, "Invalid reply type")
                             .withOperation("loadAll")
                             .withResponse(response);
            }
            else
            {
                // Clear existing cache before loading new data
                summaryCache_.clear();

                for (size_t i = 0; i < reply->elements; i++)
                {
                    std::string key = reply->element[i]->str;
                    domain::SummaryData data;
                    if (find(key, data))
                    {
                        result.push_back(data);
                        // Initialize cache with loaded data
                        summaryCache_[key] = data;
                    }
                }
                status = domain::Status::ok()
                             .withOperation("loadAll")
                             .withResponse(response);
            }

            if (reply)
                freeReplyObject(reply);

            if (!status.isOk())
            {
                LOG_F(ERROR, "%s", status.toString().c_str());
            }
            else
            {
                LOG_F(INFO, "%s", status.toString().c_str());
            }
        }
        catch (const std::exception &ex)
        {
            auto status = domain::Status::error(domain::Status::Code::RedisError, ex.what())
                              .withOperation("loadAll");
            LOG_F(ERROR, "%s", status.toString().c_str());
        }
        return result;
    }

    std::map<std::string, domain::SummaryData> RedisSummaryAdapter::getAllMapped()
    {
        return summaryCache_;
    }

    bool RedisSummaryAdapter::update(const std::string &key, const domain::SummaryData &data)
    {
        (void)key; // Mark parameter as intentionally unused
        return save(data);
    }

    bool RedisSummaryAdapter::updateCompanySummary(const std::string &stock_id)
    {
        try
        {
            if (!redisContext_)
            {
                auto status = connect();
                if (!status.isOk())
                {
                    LOG_F(ERROR, "%s", status.toString().c_str());
                    return false;
                }
            }

            // Get all summaries for this stock
            auto summaries = getAllBySecondaryKey(stock_id);
            if (summaries.empty())
            {
                auto status = domain::Status::error(domain::Status::Code::NotFound, "No summaries found")
                                  .withOperation("updateCompanySummary")
                                  .withKey(stock_id);
                LOG_F(WARNING, "%s", status.toString().c_str());
                return false;
            }

            // Create aggregated summary
            domain::SummaryData aggregated;
            aggregated.stockId = stock_id;
            aggregated.areaCenter = "ALL";

            // Aggregate all values
            for (const auto &summary : summaries)
            {
                aggregated.marginAvailableAmount += summary.marginAvailableAmount;
                aggregated.marginAvailableQty += summary.marginAvailableQty;
                aggregated.shortAvailableAmount += summary.shortAvailableAmount;
                aggregated.shortAvailableQty += summary.shortAvailableQty;
                aggregated.afterMarginAvailableAmount += summary.afterMarginAvailableAmount;
                aggregated.afterMarginAvailableQty += summary.afterMarginAvailableQty;
                aggregated.afterShortAvailableAmount += summary.afterShortAvailableAmount;
                aggregated.afterShortAvailableQty += summary.afterShortAvailableQty;
                aggregated.margin_buy_offset_qty += summary.margin_buy_offset_qty;
                aggregated.short_sell_offset_qty += summary.short_sell_offset_qty;
            }

            // Get all branches
            aggregated.belongBranches = areaBranchProvider_->getAllBranches();

            // Save aggregated summary
            std::string key = "summary:ALL:" + stock_id;
            std::string json = serializeSummaryData(aggregated);

            redisReply *reply = (redisReply *)redisCommand(redisContext_, "SET %s %s", key.c_str(), json.c_str());
            std::string response = reply ? reply->str : "";

            domain::Status status = reply && reply->type != REDIS_REPLY_ERROR
                                        ? domain::Status::ok()
                                              .withOperation("updateCompanySummary")
                                              .withKey(key)
                                              .withRequest(json)
                                              .withResponse(response)
                                        : domain::Status::error(domain::Status::Code::RedisError, reply ? reply->str : "no reply")
                                              .withOperation("updateCompanySummary")
                                              .withKey(key)
                                              .withRequest(json)
                                              .withResponse(response);

            if (reply)
                freeReplyObject(reply);

            if (!status.isOk())
            {
                LOG_F(ERROR, "%s", status.toString().c_str());
            }
            else
            {
                LOG_F(INFO, "%s", status.toString().c_str());
            }

            // Update cache
            summaryCache_[key] = aggregated;
            return status.isOk();
        }
        catch (const std::exception &ex)
        {
            auto status = domain::Status::error(domain::Status::Code::RedisError, ex.what())
                              .withOperation("updateCompanySummary")
                              .withKey(stock_id);
            LOG_F(ERROR, "%s", status.toString().c_str());
            return false;
        }
    }

    std::string RedisSummaryAdapter::serializeSummaryData(const domain::SummaryData &data) const
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
        return j.dump();
    }

    domain::Status RedisSummaryAdapter::deserializeSummaryData(const std::string &json, domain::SummaryData &data) const
    {
        try
        {
            auto j = nlohmann::json::parse(json);
            data.stockId = j["stock_id"].get<std::string>();
            data.areaCenter = j["area_center"].get<std::string>();
            data.marginAvailableAmount = j["margin_available_amount"].get<double>();
            data.marginAvailableQty = j["margin_available_qty"].get<int64_t>();
            data.shortAvailableAmount = j["short_available_amount"].get<double>();
            data.shortAvailableQty = j["short_available_qty"].get<int64_t>();
            data.afterMarginAvailableAmount = j["after_margin_available_amount"].get<double>();
            data.afterMarginAvailableQty = j["after_margin_available_qty"].get<int64_t>();
            data.afterShortAvailableAmount = j["after_short_available_amount"].get<double>();
            data.afterShortAvailableQty = j["after_short_available_qty"].get<int64_t>();
            data.belongBranches = j["belong_branches"].get<std::vector<std::string>>();
            return domain::Status::ok()
                .withOperation("deserializeSummaryData")
                .withRequest(json);
        }
        catch (const std::exception &ex)
        {
            return domain::Status::error(domain::Status::Code::DeserializationError, ex.what())
                .withOperation("deserializeSummaryData")
                .withRequest(json);
        }
    }

    std::vector<domain::SummaryData> RedisSummaryAdapter::getAllBySecondaryKey(const std::string &secondaryKey)
    {
        std::vector<domain::SummaryData> result;
        if (!redisContext_)
            return result;

        try
        {
            redisReply *reply = (redisReply *)redisCommand(redisContext_, "KEYS summary:*");
            std::string response = reply ? reply->str : "";

            domain::Status status;
            if (!reply || reply->type != REDIS_REPLY_ARRAY)
            {
                status = domain::Status::error(domain::Status::Code::RedisError, "Invalid reply type")
                             .withOperation("getAllBySecondaryKey")
                             .withKey(secondaryKey)
                             .withResponse(response);
            }
            else
            {
                for (size_t i = 0; i < reply->elements; i++)
                {
                    std::string key = reply->element[i]->str;
                    domain::SummaryData data;
                    if (find(key, data) && data.areaCenter == secondaryKey)
                    {
                        result.push_back(data);
                    }
                }
                status = domain::Status::ok()
                             .withOperation("getAllBySecondaryKey")
                             .withKey(secondaryKey)
                             .withResponse(response);
            }

            if (reply)
                freeReplyObject(reply);

            if (!status.isOk())
            {
                LOG_F(ERROR, "%s", status.toString().c_str());
            }
            else
            {
                LOG_F(INFO, "%s", status.toString().c_str());
            }
        }
        catch (const std::exception &ex)
        {
            auto status = domain::Status::error(domain::Status::Code::RedisError, ex.what())
                              .withOperation("getAllBySecondaryKey")
                              .withKey(secondaryKey);
            LOG_F(ERROR, "%s", status.toString().c_str());
        }
        return result;
    }

    bool RedisSummaryAdapter::createIndex()
    {
        if (!redisContext_)
            return false;

        try
        {
            redisReply *reply = (redisReply *)redisCommand(redisContext_,
                                                           "FT.CREATE outputIdx ON JSON PREFIX 1 summary: SCHEMA "
                                                           "$.stock_id AS stock_id TEXT "
                                                           "$.area_center AS area_center TEXT "
                                                           "$.belong_branches.* AS branches TAG");

            std::string response = reply ? reply->str : "";

            domain::Status status = reply && reply->type != REDIS_REPLY_ERROR
                                        ? domain::Status::ok()
                                              .withOperation("createIndex")
                                              .withResponse(response)
                                        : domain::Status::error(domain::Status::Code::RedisError, reply ? reply->str : "no reply")
                                              .withOperation("createIndex")
                                              .withResponse(response);

            if (reply)
                freeReplyObject(reply);

            if (!status.isOk())
            {
                LOG_F(ERROR, "%s", status.toString().c_str());
            }
            else
            {
                LOG_F(INFO, "%s", status.toString().c_str());
            }

            return status.isOk();
        }
        catch (const std::exception &ex)
        {
            auto status = domain::Status::error(domain::Status::Code::RedisError, ex.what())
                              .withOperation("createIndex");
            LOG_F(ERROR, "%s", status.toString().c_str());
            return false;
        }
    }

    domain::SummaryData *RedisSummaryAdapter::get(const std::string &key)
    {
        // Check cache first
        auto it = summaryCache_.find(key);
        if (it != summaryCache_.end())
        {
            return new domain::SummaryData(it->second);
        }

        // Not in cache, check Redis
        auto data = std::make_unique<domain::SummaryData>();
        if (find(key, *data))
        {
            // Update cache
            summaryCache_[key] = *data;
            return data.release();
        }
        return nullptr;
    }

} // namespace finance::infrastructure::storage