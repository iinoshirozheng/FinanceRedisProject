#include "RedisSummaryAdapter.h"
#include "../../common/FinanceUtils.hpp"
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
            LOG_F(ERROR, "Failed to connect to Redis: %s", status.message().c_str());
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
                return domain::Status::ok();
            }

            redisContext_ = redisConnect(configProvider_->getRedisUrl().c_str(), 6379);
            if (!redisContext_ || redisContext_->err)
            {
                std::string error = redisContext_ ? redisContext_->errstr : "Failed to allocate Redis context";
                return domain::Status::error(domain::Status::Code::CONNECTION_ERROR, error);
            }

            return domain::Status::ok();
        }
        catch (const std::exception &ex)
        {
            return domain::Status::error(domain::Status::Code::CONNECTION_ERROR, ex.what());
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
                    return false;
                }
            }

            std::string key = common::FinanceUtils::generateKey(data);
            std::string json = serializeSummaryData(data);

            redisReply *reply = (redisReply *)redisCommand(redisContext_, "SET %s %s", key.c_str(), json.c_str());
            if (!reply)
            {
                return false;
            }

            freeReplyObject(reply);

            // 更新 cache
            summaryCache_[key] = data;
            return true;
        }
        catch (const std::exception &ex)
        {
            LOG_F(ERROR, "Redis save error: %s", ex.what());
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
                    return false;
                }
            }

            redisReply *reply = (redisReply *)redisCommand(redisContext_, "GET %s", key.c_str());
            if (!reply)
            {
                return false;
            }

            if (reply->type == REDIS_REPLY_NIL)
            {
                freeReplyObject(reply);
                return false;
            }

            auto status = deserializeSummaryData(reply->str, data);
            freeReplyObject(reply);
            return status.isOk();
        }
        catch (const std::exception &ex)
        {
            LOG_F(ERROR, "Redis find error: %s", ex.what());
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
                    return false;
                }
            }

            redisReply *reply = (redisReply *)redisCommand(redisContext_, "DEL %s", key.c_str());
            if (!reply)
            {
                return false;
            }

            freeReplyObject(reply);

            // 從 cache 刪除
            summaryCache_.erase(key);
            return true;
        }
        catch (const std::exception &ex)
        {
            LOG_F(ERROR, "Redis remove error: %s", ex.what());
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
            if (!reply || reply->type != REDIS_REPLY_ARRAY)
            {
                if (reply)
                    freeReplyObject(reply);
                return result;
            }

            for (size_t i = 0; i < reply->elements; i++)
            {
                std::string key = reply->element[i]->str;
                domain::SummaryData data;
                if (find(key, data))
                {
                    result.push_back(data);
                }
            }
            freeReplyObject(reply);
        }
        catch (const std::exception &ex)
        {
            LOG_F(ERROR, "Redis loadAll error: %s", ex.what());
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
        (void)stock_id; // Mark parameter as intentionally unused
        return false;   // TODO: Implement company summary update
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
            return domain::Status::ok();
        }
        catch (const std::exception &ex)
        {
            return domain::Status::error(domain::Status::Code::DESERIALIZATION_ERROR, ex.what());
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
            if (!reply || reply->type != REDIS_REPLY_ARRAY)
            {
                if (reply)
                    freeReplyObject(reply);
                return result;
            }

            for (size_t i = 0; i < reply->elements; i++)
            {
                std::string key = reply->element[i]->str;
                domain::SummaryData data;
                if (find(key, data) && data.areaCenter == secondaryKey)
                {
                    result.push_back(data);
                }
            }
            freeReplyObject(reply);
        }
        catch (const std::exception &ex)
        {
            LOG_F(ERROR, "Redis getAllBySecondaryKey error: %s", ex.what());
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

            bool success = (reply != nullptr && reply->type != REDIS_REPLY_ERROR);
            if (reply)
                freeReplyObject(reply);
            return success;
        }
        catch (const std::exception &ex)
        {
            LOG_F(ERROR, "Redis createIndex error: %s", ex.what());
            return false;
        }
    }

    domain::SummaryData *RedisSummaryAdapter::get(const std::string &key)
    {
        // 先查 cache
        auto it = summaryCache_.find(key);
        if (it != summaryCache_.end())
        {
            return new domain::SummaryData(it->second);
        }

        // cache 沒有，去 Redis 查
        auto data = std::make_unique<domain::SummaryData>();
        if (find(key, *data))
        {
            // 更新 cache
            summaryCache_[key] = *data;
            return data.release();
        }
        return nullptr;
    }

} // namespace finance::infrastructure::storage