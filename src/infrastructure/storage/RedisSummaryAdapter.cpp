// infrastructure/storage/RedisSummaryAdapter.cpp
#include "RedisSummaryAdapter.h"
#include <loguru.hpp>
#include "../config/ConnectionConfigProvider.hpp"
#include "../config/AreaBranchProvider.hpp"
#include "../../domain/Result.hpp"

namespace finance::infrastructure::storage
{
    using finance::domain::ErrorCode;
    using finance::domain::ErrorResult;
    using finance::domain::Result;
    namespace config = finance::infrastructure::config;

    /**
     * @brief 初始化 Redis 連接
     */
    Result<void, ErrorResult> RedisSummaryAdapter::init()
    {
        if (redisClient_)
            return Result<void, ErrorResult>::Ok();

        const std::string uri = config::ConnectionConfigProvider::redisUri();
        const size_t poolSize = config::ConnectionConfigProvider::redisPoolSize();
        const int waitTimeoutMs = config::ConnectionConfigProvider::redisWaitTimeoutMs();
        auto client = std::make_unique<RedisPlusPlusClient<SummaryData, ErrorResult>>();

        return client->setConnectOption(poolSize, waitTimeoutMs)
            .and_then([client = std::move(client), uri]() mutable
                      { return client->connect(uri); })
            .and_then([this, client = std::move(client)]() mutable
                      {
                this->redisClient_ = std::move(client);
                return Result<void, ErrorResult>::Ok(); })
            .map_err([](const ErrorResult &e)
                     { return ErrorResult{ErrorCode::RedisConnectionFailed,
                                          "Redis 連線失敗: " + e.message}; });
    }

    /**
     * @brief 更新本地緩存
     */
    Result<void, ErrorResult> RedisSummaryAdapter::setCacheData(
        const std::string &key, SummaryData data)
    {
        summaryCacheData_[key] = std::move(data);
        return Result<void, ErrorResult>::Ok();
    }

    /**
     * @brief 序列化並同步資料到 Redis，同時更新本地緩存。
     */
    Result<void, ErrorResult> RedisSummaryAdapter::sync(const SummaryData &d)
    {
        if (!redisClient_)
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisConnectionFailed, "Redis 未正確連線"});

        const auto key = makeKey(d);
        return summaryDataToJson(d)
            .and_then([this, key](const std::string &j)
                      { return redisClient_->setJson(key, "$", j); })
            .and_then([this, key, d]
                      { return setCacheData(key, d); })
            .map_err([&](const ErrorResult &e)
                     { return ErrorResult{e.code, "Sync 失敗: " + e.message}; });
    }

    /**
     * @brief 從本地快取或 Redis 中取得資料。
     */
    Result<SummaryData, ErrorResult> RedisSummaryAdapter::get(
        const std::string &key)
    {
        if (!redisClient_)
            return Result<SummaryData, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisConnectionFailed, "Redis 未正確連線"});

        // 1) 本地快取命中
        if (auto cached = getCachedData(key); cached)
        {
            return Result<SummaryData, ErrorResult>::Ok(*cached);
        }

        // 2) 快取未命中，從 Redis 讀取
        return redisClient_->getJson(key, "$")
            .and_then([this](const std::string &jsonStr)
                      { return jsonToSummaryData(jsonStr); })
            .and_then([this, key](const SummaryData &d)
                      { return setCacheData(key, d)
                            .and_then([d]
                                      { return Result<SummaryData, ErrorResult>::Ok(d); }); })
            .map_err([&](const ErrorResult &e)
                     { return ErrorResult{e.code, "Get 失敗 [" + key + "]: " + e.message}; });
    }

    /**
     * @brief 從 Redis 加載所有符合模式的資料到本地緩存。
     */
    Result<void, ErrorResult> RedisSummaryAdapter::loadAll()
    {
        if (!redisClient_)
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisConnectionFailed, "Redis 未正確連線"});

        const auto pattern = "summary:*";
        return redisClient_->keys(pattern)
            .and_then([this](const std::vector<std::string> &keys)
                      { return loadAndCacheKeysData(keys); })
            .map_err([](const ErrorResult &e)
                     { return ErrorResult{e.code, "LoadAll 操作失敗: " + e.message}; });
    }

    Result<void, ErrorResult> RedisSummaryAdapter::loadAndCacheKeysData(
        const std::vector<std::string> &keys)
    {
        summaryCacheData_.clear();
        size_t loaded = 0;
        for (const auto &key : keys)
        {
            auto jsonRes = redisClient_->getJson(key, "$");
            if (jsonRes.is_err())
            {
                LOG_F(WARNING, "JSON.GET '%s' 失敗: %s",
                      key.c_str(), jsonRes.unwrap_err().message.c_str());
                continue;
            }

            auto parseRes = jsonToSummaryData(jsonRes.unwrap());
            if (parseRes.is_err())
            {
                LOG_F(WARNING, "解析 '%s' JSON 失敗: %s",
                      key.c_str(), parseRes.unwrap_err().message.c_str());
                continue;
            }

            summaryCacheData_[key] = parseRes.unwrap();
            loaded++;
        }
        LOG_F(INFO, "已從 Redis 載入 %zu 筆 summary 資料。", loaded);
        return Result<void, ErrorResult>::Ok();
    }

    /**
     * @brief 將 SummaryData 序列化為 JSON 字串。
     */
    Result<std::string, ErrorResult> RedisSummaryAdapter::summaryDataToJson(
        const SummaryData &data)
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
            return Result<std::string, ErrorResult>::Ok(j.dump());
        }
        catch (const std::exception &ex)
        {
            return Result<std::string, ErrorResult>::Err(
                ErrorResult{ErrorCode::JsonParseError, ex.what()});
        }
    }

    /**
     * @brief 將 JSON 字串反序列化為 SummaryData。
     */
    Result<SummaryData, ErrorResult> RedisSummaryAdapter::jsonToSummaryData(
        const std::string &jsonStr)
    {
        try
        {
            nlohmann::json j = nlohmann::json::parse(jsonStr);
            if (j.is_array() && !j.empty())
                j = j[0];

            SummaryData data;
            data.stock_id = j.at("stock_id").get<std::string>();
            data.area_center = j.at("area_center").get<std::string>();
            data.margin_available_amount = j.at("margin_available_amount").get<int64_t>();
            data.margin_available_qty = j.at("margin_available_qty").get<int64_t>();
            data.short_available_amount = j.at("short_available_amount").get<int64_t>();
            data.short_available_qty = j.at("short_available_qty").get<int64_t>();
            data.after_margin_available_amount = j.at("after_margin_available_amount").get<int64_t>();
            data.after_margin_available_qty = j.at("after_margin_available_qty").get<int64_t>();
            data.after_short_available_amount = j.at("after_short_available_amount").get<int64_t>();
            data.after_short_available_qty = j.at("after_short_available_qty").get<int64_t>();
            data.belong_branches = j.at("belong_branches").get<std::vector<std::string>>();
            return Result<SummaryData, ErrorResult>::Ok(data);
        }
        catch (const std::exception &ex)
        {
            return Result<SummaryData, ErrorResult>::Err(
                ErrorResult{ErrorCode::JsonParseError,
                            "解析JSON失敗: " + std::string(ex.what())});
        }
    }

    /**
     * @brief 將 SummaryData 的 key 序列化為 Redis key
     */
    std::string RedisSummaryAdapter::makeKey(const SummaryData &data)
    {
        return "summary:" + data.stock_id + ":" + data.area_center;
    }

    /**
     * @brief 實現 remove 方法
     */
    bool RedisSummaryAdapter::remove(const std::string &key)
    {
        if (!redisClient_)
            return false;
        auto res = redisClient_->del(key);
        if (res.is_ok())
        {
            summaryCacheData_.erase(key);
            return true;
        }
        return false;
    }

    /**
     * @brief 公司層級彙總: ALL summary
     *        使用本地快取與 AreaBranchProvider 定義的 backOfficeIds
     */
    Result<void, ErrorResult> RedisSummaryAdapter::updateCompanySummary(
        const std::string &stock_id)
    {
        if (!redisClient_)
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisConnectionFailed, "Redis 未正確連線"});

        SummaryData company_summary;
        company_summary.stock_id = stock_id;
        company_summary.area_center = "ALL";

        // 取得所有 backOfficeId
        auto backofficeIds = config::AreaBranchProvider::getBackofficeIds();

        {
            for (const auto &officeId : backofficeIds)
            {
                const std::string key = "summary:" + officeId + ":" + stock_id;
                if (auto it = summaryCacheData_.find(key); it != summaryCacheData_.end())
                {
                    const auto &d = it->second;
                    company_summary.margin_available_amount += d.margin_available_amount;
                    company_summary.margin_available_qty += d.margin_available_qty;
                    company_summary.short_available_amount += d.short_available_amount;
                    company_summary.short_available_qty += d.short_available_qty;
                    company_summary.after_margin_available_amount += d.after_margin_available_amount;
                    company_summary.after_margin_available_qty += d.after_margin_available_qty;
                    company_summary.after_short_available_amount += d.after_short_available_amount;
                    company_summary.after_short_available_qty += d.after_short_available_qty;
                }
            }
        }

        return sync(company_summary);
    }

    /**
     * @brief 取得本地緩存資料
     */
    std::optional<SummaryData> RedisSummaryAdapter::getCachedData(
        const std::string &key) const
    {
        if (auto it = summaryCacheData_.find(key); it != summaryCacheData_.end())
            return it->second;
        return std::nullopt;
    }

    Result<void, ErrorResult> RedisSummaryAdapter::update(const std::string &key)
    {
        throw std::runtime_error("RedisSummaryAdapter::update not implemented");
    }

    Result<void, ErrorResult> RedisSummaryAdapter::set(const std::string &key)
    {
        throw std::runtime_error("RedisSummaryAdapter::set not implemented");
    }

} // namespace finance::infrastructure::storage
