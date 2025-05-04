#include "RedisSummaryAdapter.h"
#include "../../utils/FinanceUtils.hpp"
#include <loguru.hpp>
#include <nlohmann/json.hpp>
#include <hiredis/hiredis.h>

namespace finance::infrastructure::storage
{
    const std::string RedisSummaryAdapter::KEY_PREFIX = "summary";
    redisContext *RedisSummaryAdapter::redisContext_ = nullptr;
    std::map<std::string, domain::SummaryData> RedisSummaryAdapter::summaryCache_;
    std::mutex RedisSummaryAdapter::cacheMutex_;

    RedisSummaryAdapter::RedisSummaryAdapter()
    {
        auto status = connectToRedis();
        if (!status.isOk())
        {
            LOG_F(ERROR, "%s", status.toString().c_str());
        }
    }

    RedisSummaryAdapter::~RedisSummaryAdapter()
    {
        disconnectToRedis();
    }

    bool RedisSummaryAdapter::init()
    {
        auto status = connectToRedis();
        if (!status.isOk())
        {
            LOG_F(ERROR, "%s", status.toString().c_str());
            return false;
        }
        return true;
    }

    domain::Status RedisSummaryAdapter::connectToRedis()
    {
        try
        {
            if (redisContext_)
            {
                return domain::Status::ok()
                    .withOperation("connect")
                    .withResponse("Already connected");
            }

            redisContext_ = redisConnect(config::ConnectionConfigProvider::redisUrl().c_str(), 6479);
            if (!redisContext_ || redisContext_->err)
            {
                std::string error = redisContext_ ? redisContext_->errstr : "Failed to allocate Redis context";
                return domain::Status::error(domain::Status::Code::ConnectionError, error)
                    .withOperation("connect")
                    .withRequest(config::ConnectionConfigProvider::redisUrl());
            }

            return domain::Status::ok()
                .withOperation("connect")
                .withRequest(config::ConnectionConfigProvider::redisUrl());
        }
        catch (const std::exception &ex)
        {
            return domain::Status::error(domain::Status::Code::ConnectionError, ex.what())
                .withOperation("connect")
                .withRequest(config::ConnectionConfigProvider::redisUrl());
        }
    }

    void RedisSummaryAdapter::disconnectToRedis()
    {
        if (redisContext_)
        {
            redisFree(redisContext_);
            redisContext_ = nullptr;
        }
    }

    bool RedisSummaryAdapter::setData(const std::string &key, const domain::SummaryData &data)
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto result = summaryCache_.insert({key, data});
        if (!result.second)
        {
            summaryCache_[key] = data;
            return true;
        }
        return false;
    }

    domain::Status RedisSummaryAdapter::getSummaryDataFromRedis(const std::string &key, domain::SummaryData &data)
    {
        try
        {
            domain::Status status = connectToRedis();
            if (!status.isOk())
            {
                return status;
            }

            redisReply *reply = (redisReply *)redisCommand(redisContext_, "GET %s", key.c_str());
            std::string response = reply ? reply->str : "";

            domain::Status result;
            if (!reply)
            {
                result = domain::Status::error(domain::Status::Code::RedisError, "No reply")
                             .withOperation("find")
                             .withKey(key)
                             .withResponse(response);
            }
            else if (reply->type == REDIS_REPLY_NIL)
            {
                result = domain::Status::error(domain::Status::Code::NotFound, "Key not found")
                             .withOperation("find")
                             .withKey(key)
                             .withResponse(response);
            }
            else
            {
                result = deserializeSummaryData(reply->str, data);
                result.withOperation("find")
                    .withKey(key)
                    .withResponse(response);
            }

            if (reply)
                freeReplyObject(reply);

            if (!result.isOk())
            {
                LOG_F(ERROR, "%s", result.toString().c_str());
            }
            else
            {
                LOG_F(INFO, "%s", result.toString().c_str());
            }

            return result;
        }
        catch (const std::exception &ex)
        {
            auto status = domain::Status::error(domain::Status::Code::RedisError, ex.what())
                              .withOperation("find")
                              .withKey(key);
            LOG_F(ERROR, "%s", status.toString().c_str());
            return status;
        }
    }

    bool RedisSummaryAdapter::removeData(const std::string &key)
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        size_t removed = summaryCache_.erase(key);

        if (removed > 0)
        {
            LOG_F(INFO, "Successfully to removed key: %s", key.c_str());
            return true;
        }
        else
        {
            LOG_F(INFO, "Failed to removed key: %s", key.c_str());
            return false;
        }
    }

    domain::Status RedisSummaryAdapter::loadAllFromRedis()
    {
        // 1. 連接 Redis
        domain::Status status = connectToRedis();
        if (!status.isOk())
        {
            return status; // 如果無法連接，返回錯誤
        }

        try
        {
            // 2. 執行 Redis 命令，查詢所有以 "summary:*" 開頭的鍵
            redisReply *reply = static_cast<redisReply *>(redisCommand(redisContext_, "KEYS summary:*"));

            // 3. 驗證 reply 是否正確
            if (!reply || reply->type != REDIS_REPLY_ARRAY)
            {
                std::string response = reply ? reply->str : "Null reply or invalid type";
                if (reply)
                    freeReplyObject(reply);
                return domain::Status::error(domain::Status::Code::RedisError, "Invalid reply type")
                    .withOperation("loadAll")
                    .withResponse(response);
            }

            // 4. 提取所有 keys
            std::vector<std::string> keys;
            for (size_t i = 0; i < reply->elements; i++)
            {
                keys.emplace_back(reply->element[i]->str);
            }
            freeReplyObject(reply); // 釋放 reply

            // 5. 清空緩存
            {
                std::lock_guard<std::mutex> lock(cacheMutex_);
                summaryCache_.clear();
            }

            // 6. 加載每個鍵對應的數據
            for (const auto &key : keys)
            {
                domain::SummaryData data;

                domain::Status status = findSummaryDataFromRedis(key, data);
                LOG_F(INFO, "%s", status.toString().c_str());

                std::lock_guard<std::mutex> lock(cacheMutex_);
                summaryCache_[key] = data;
            }

            // 7. 返回成功狀態
            LOG_F(INFO, "Successfully loaded %lu keys from Redis.", keys.size());
            return domain::Status::ok()
                .withOperation("loadAll")
                .withResponse("Successfully loaded data from Redis");
        }
        catch (const std::exception &ex)
        {
            // 捕獲任何例外並返回錯誤狀態
            auto status = domain::Status::error(domain::Status::Code::RedisError, ex.what())
                              .withOperation("loadAll");
            LOG_F(ERROR, "%s", status.toString().c_str());
            return status;
        }
        return status;
    }

    std::map<std::string, domain::SummaryData> &RedisSummaryAdapter::getAllMapped()
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        return summaryCache_;
    }

    bool RedisSummaryAdapter::updateData(const std::string &key, const domain::SummaryData &data)
    {
        return setData(key, data);
    }

    bool RedisSummaryAdapter::updateCompanySummary(const std::string &stock_id)
    {
        domain::SummaryData company_summary;
        for (const auto &backoffice_id : config::AreaBranchProvider::getBackofficeIds())
        {
            std::string key = KEY_PREFIX + ":" + backoffice_id + ":" + stock_id;

            if (domain::SummaryData *area_summary_data = getData(key))
            {
                company_summary.area_center = "ALL";
                company_summary.stock_id = area_summary_data->stock_id;
                company_summary.belong_branches = config::AreaBranchProvider::getAllBranches();
                company_summary.margin_available_amount += area_summary_data->margin_available_amount;
                company_summary.margin_available_qty += area_summary_data->margin_available_qty;
                company_summary.short_available_amount += area_summary_data->short_available_amount;
                company_summary.short_available_qty += area_summary_data->short_available_qty;
                company_summary.after_margin_available_amount += area_summary_data->after_margin_available_amount;
                company_summary.after_margin_available_qty += area_summary_data->after_margin_available_qty;
                company_summary.after_short_available_amount += area_summary_data->after_short_available_amount;
                company_summary.after_short_available_qty += area_summary_data->after_short_available_qty;
            }
        }

        return syncToRedis(company_summary).isOk();
    }

    domain::Status RedisSummaryAdapter::serializeSummaryData(const domain::SummaryData &data, std::string &out_dump)
    {
        try
        {
            nlohmann::json j;
            j["stock_id"] = data.stock_id;
            j["area_center"] = data.area_center;
            j["margin_available_amount"] = data.margin_available_amount;
            j["margin_available_qty"] = data.margin_available_qty;
            j["short_available_amount"] = data.short_available_amount;
            j["short_available_qty"] = data.short_available_qty;
            j["after_margin_available_amount"] = data.after_margin_available_amount;
            j["after_margin_available_qty"] = data.after_margin_available_qty;
            j["after_short_available_amount"] = data.after_short_available_amount;
            j["after_short_available_qty"] = data.after_short_available_qty;
            j["belong_branches"] = data.belong_branches;
            out_dump = j.dump();
            return domain::Status::ok()
                .withOperation("serializeSummaryData")
                .withRequest(out_dump);
        }
        catch (const std::exception &ex)
        {
            return domain::Status::error(domain::Status::Code::DeserializationError, ex.what())
                .withOperation("serializeSummaryData");
        }
    }

    domain::Status RedisSummaryAdapter::deserializeSummaryData(const std::string &json, domain::SummaryData &out_data)
    {
        try
        {
            auto j = nlohmann::json::parse(json);
            out_data.stock_id = j["stock_id"].get<std::string>();
            out_data.area_center = j["area_center"].get<std::string>();
            out_data.margin_available_amount = j["margin_available_amount"].get<double>();
            out_data.margin_available_qty = j["margin_available_qty"].get<int64_t>();
            out_data.short_available_amount = j["short_available_amount"].get<double>();
            out_data.short_available_qty = j["short_available_qty"].get<int64_t>();
            out_data.after_margin_available_amount = j["after_margin_available_amount"].get<double>();
            out_data.after_margin_available_qty = j["after_margin_available_qty"].get<int64_t>();
            out_data.after_short_available_amount = j["after_short_available_amount"].get<double>();
            out_data.after_short_available_qty = j["after_short_available_qty"].get<int64_t>();
            out_data.belong_branches = j["belong_branches"].get<std::vector<std::string>>();
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

    domain::Status RedisSummaryAdapter::syncToRedis(const domain::SummaryData &data)
    {
        try
        {
            if (!redisContext_)
            {
                return domain::Status::error(domain::Status::Code::ConnectionError, "Not connected to Redis");
            }

            std::string out_key = utils::FinanceUtils::generateKey(KEY_PREFIX, data);
            std::string out_json;
            serializeSummaryData(data, out_json);

            redisReply *reply = (redisReply *)redisCommand(redisContext_, "SET %s %s", out_key.c_str(), out_json.c_str());
            std::string response = reply ? reply->str : "redisReply == nullptr";

            domain::Status status = reply && reply->type != REDIS_REPLY_ERROR
                                        ? domain::Status::ok()
                                              .withOperation("save to redis")
                                              .withKey(out_key)
                                              .withRequest(out_json)
                                              .withResponse(response)
                                        : domain::Status::error(domain::Status::Code::RedisError, reply ? reply->str : "no reply")
                                              .withOperation("save to redis")
                                              .withKey(out_key)
                                              .withRequest(out_json)
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

            std::lock_guard<std::mutex> lock(cacheMutex_);
            summaryCache_[out_key] = data;
            return status;
        }
        catch (const std::exception &ex)
        {
            auto status = domain::Status::error(domain::Status::Code::RedisError, ex.what())
                              .withOperation("set");
            LOG_F(ERROR, "%s", status.toString().c_str());
            return status;
        }
    }

    domain::Status RedisSummaryAdapter::removeSummaryDataFromRedis(const std::string &key)
    {
        try
        {
            domain::Status status = connectToRedis();
            if (!status.isOk())
            {
                return status; // 如果無法連接，返回錯誤
            }

            redisReply *reply = (redisReply *)redisCommand(redisContext_, "DEL %s", key.c_str());
            std::string response = reply ? reply->str : "";

            status = reply && reply->type != REDIS_REPLY_ERROR
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
            std::lock_guard<std::mutex> lock(cacheMutex_);
            summaryCache_.erase(key);
            return status;
        }
        catch (const std::exception &ex)
        {
            auto status = domain::Status::error(domain::Status::Code::RedisError, ex.what())
                              .withOperation("removeSummaryDataFromRedis")
                              .withKey(key);
            LOG_F(ERROR, "%s", status.toString().c_str());
            return status;
        }
    }

    domain::Status RedisSummaryAdapter::findSummaryDataFromRedis(const std::string &key, domain::SummaryData &out_data)
    {
        try
        {
            domain::Status status = connectToRedis();
            if (!status.isOk())
            {
                return status; // 如果無法連接，返回錯誤
            }

            // 1. 从 Redis 中执行 JSON.GET 命令获取数据（假设数据以 JSON 格式存储）
            redisReply *reply = static_cast<redisReply *>(redisCommand(redisContext_, "JSON.GET %s", key.c_str()));

            // 2. 检查 reply 是否有效
            if (!reply || reply->type != REDIS_REPLY_STRING)
            {
                if (reply)
                    freeReplyObject(reply);
                LOG_F(WARNING, "Failed to fetch data for key '%s'. Reply was null or not a string.", key.c_str());
                return domain::Status::error(domain::Status::Code::RedisError, reply ? reply->str : "no reply")
                    .withOperation("remove")
                    .withKey(key);
            }

            // 3. 解析 JSON 返回的数据
            std::string json_str(reply->str);
            freeReplyObject(reply); // 确保释放资源

            return deserializeSummaryData(json_str, out_data);
        }
        catch (const std::exception &ex)
        {
            auto status = domain::Status::error(domain::Status::Code::RedisError, ex.what())
                              .withOperation("findSummaryDataFromRedis")
                              .withKey(key);
            LOG_F(ERROR, "%s", status.toString().c_str());
            return status;
        }
    }

    bool RedisSummaryAdapter::createRedisTableIndex()
    {
        domain::Status status = connectToRedis();
        if (!status.isOk())
        {
            return false; // 如果無法連接，返回錯誤
        }

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
                                              .withOperation("createRedisTableIndex")
                                              .withResponse(response)
                                        : domain::Status::error(domain::Status::Code::RedisError, reply ? reply->str : "no reply")
                                              .withOperation("createRedisTableIndex")
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
                              .withOperation("createRedisTableIndex");
            LOG_F(ERROR, "%s", status.toString().c_str());
            return false;
        }
    }

    domain::SummaryData *RedisSummaryAdapter::getData(const std::string &key)
    {
        std::lock_guard<std::mutex> lock(cacheMutex_);
        auto it = summaryCache_.find(key);
        if (it != summaryCache_.end())
        {
            return &it->second;
        }
        return nullptr;
    }
} // namespace finance::infrastructure::storage