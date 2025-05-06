// RedisSummaryAdapter.cpp
#include "RedisSummaryAdapter.h"
#include "RedisClient.hpp"
#include <loguru.hpp>

namespace finance::infrastructure::storage
{
    using finance::domain::Result;

    Result<void, ResultError> RedisSummaryAdapter::init()
    {
        if (ctx_)
            return Result<void, ResultError>::Ok();

        auto conn = connect_redis(
            config::ConnectionConfigProvider::redisUrl(),
            config::ConnectionConfigProvider::serverPort());
        if (conn.is_err())
            return Result<void, ResultError>::Err(conn.unwrap_err());

        ctx_ = std::move(conn.unwrap());
        return Result<void, ResultError>::Ok();
    }

    Result<void, ResultError> RedisSummaryAdapter::sync(const SummaryData &data)
    {
        // Lazy init
        if (!ctx_)
        {
            auto r = init();
            if (r.is_err())
                return r;
        }

        // 序列化
        auto jres = toJson(data);
        if (jres.is_err())
            return Result<void, ResultError>::Err(jres.unwrap_err());

        // 寫 Redis
        std::string key = makeKey(data);
        auto setRes = redis_set(ctx_.get(), key, jres.unwrap());
        if (setRes.is_err())
            return setRes;

        // 更新本地快取
        {
            std::unique_lock lock(cacheMutex_);
            cache_[key] = data;
        }
        return Result<void, ResultError>::Ok();
    }

    Result<SummaryData, ResultError> RedisSummaryAdapter::get(const std::string &key)
    {
        {
            std::shared_lock lock(cacheMutex_);
            if (auto it = cache_.find(key); it != cache_.end())
                return Result<SummaryData, ResultError>::Ok(it->second);
        }

        // 本地無，從 Redis 讀取
        auto jres = getJson(key);
        if (jres.is_err())
            return Result<SummaryData, ResultError>::Err(jres.unwrap_err());

        auto deser = fromJson(jres.unwrap());
        if (deser.is_err())
            return deser;

        // 更新快取
        {
            std::unique_lock lock(cacheMutex_);
            cache_[key] = deser.unwrap();
        }
        return deser;
    }

    Result<void, ResultError> RedisSummaryAdapter::remove(const std::string &key)
    {
        if (!ctx_)
        {
            auto r = init();
            if (r.is_err())
                return r;
        }

        auto delRes = redis_del(ctx_.get(), key);
        if (delRes.is_err())
            return delRes;

        {
            std::unique_lock lock(cacheMutex_);
            cache_.erase(key);
        }
        return Result<void, ResultError>::Ok();
    }

    Result<void, ResultError> RedisSummaryAdapter::loadAll()
    {
        if (!ctx_)
        {
            auto r = init();
            if (r.is_err())
                return r;
        }

        // 取所有 key
        auto keysRes = getKeys(std::string(KEY_PREFIX) + ":*");
        if (keysRes.is_err())
            return Result<void, ResultError>::Err(keysRes.unwrap_err());

        // 清快取並重載
        {
            std::unique_lock lock(cacheMutex_);
            cache_.clear();
        }

        for (auto &key : keysRes.unwrap())
        {
            auto gre = get(key);
            if (gre.is_ok())
            {
                std::unique_lock lock(cacheMutex_);
                cache_[key] = gre.unwrap();
            }
            else
            {
                LOG_F(WARNING, "Redis loadAll: key=%s failed: %s",
                      key.c_str(), gre.unwrap_err().message.c_str());
            }
        }
        return Result<void, ResultError>::Ok();
    }

    std::unordered_map<std::string, SummaryData> RedisSummaryAdapter::getAll() const
    {
        std::shared_lock lock(cacheMutex_);
        return cache_;
    }

    // private helpers

    Result<std::vector<std::string>, ResultError> RedisSummaryAdapter::getKeys(const std::string &pattern)
    {
        redisReply *r = static_cast<redisReply *>(
            redisCommand(ctx_.get(), "KEYS %s", pattern.c_str()));
        if (!r)
            return Result<std::vector<std::string>, ResultError>::Err(ResultError("KEYS failed"));
        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(r, freeReplyObject);

        if (r->type != REDIS_REPLY_ARRAY)
            return Result<std::vector<std::string>, ResultError>::Err(ResultError("KEYS unexpected reply"));

        std::vector<std::string> result;
        for (size_t i = 0; i < r->elements; ++i)
            result.emplace_back(r->element[i]->str);
        return Result<std::vector<std::string>, ResultError>::Ok(result);
    }

    Result<std::string, ResultError> RedisSummaryAdapter::getJson(const std::string &key)
    {
        // 目前僅支援 JSON.GET
        redisReply *r = static_cast<redisReply *>(
            redisCommand(ctx_.get(), "JSON.GET %s", key.c_str()));
        if (!r)
            return Result<std::string, ResultError>::Err(ResultError("JSON.GET failed"));
        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(r, freeReplyObject);

        if (r->type != REDIS_REPLY_STRING)
            return Result<std::string, ResultError>::Err(ResultError("JSON.GET unexpected reply"));
        return Result<std::string, ResultError>::Ok(std::string(r->str));
    }

    Result<std::string, ResultError> RedisSummaryAdapter::toJson(const SummaryData &d)
    {
        try
        {
            nlohmann::json j;
            j["stock_id"] = d.stock_id;
            j["area_center"] = d.area_center;
            j["margin_available_amount"] = d.margin_available_amount;
            j["margin_available_qty"] = d.margin_available_qty;
            j["short_available_amount"] = d.short_available_amount;
            j["short_available_qty"] = d.short_available_qty;
            j["after_margin_available_amount"] = d.after_margin_available_amount;
            j["after_margin_available_qty"] = d.after_margin_available_qty;
            j["after_short_available_amount"] = d.after_short_available_amount;
            j["after_short_available_qty"] = d.after_short_available_qty;
            j["belong_branches"] = d.belong_branches;
            return Result<std::string, ResultError>::Ok(j.dump());
        }
        catch (const std::exception &ex)
        {
            return Result<std::string, ResultError>::Err(ResultError(ex.what()));
        }
    }

    Result<SummaryData, ResultError> RedisSummaryAdapter::fromJson(const std::string &jsonStr)
    {
        try
        {
            auto j = nlohmann::json::parse(jsonStr);
            SummaryData d;
            d.stock_id = j.at("stock_id").get<std::string>();
            d.area_center = j.at("area_center").get<std::string>();
            d.margin_available_amount = j.at("margin_available_amount").get<int64_t>();
            d.margin_available_qty = j.at("margin_available_qty").get<int64_t>();
            d.short_available_amount = j.at("short_available_amount").get<int64_t>();
            d.short_available_qty = j.at("short_available_qty").get<int64_t>();
            d.after_margin_available_amount = j.at("after_margin_available_amount").get<int64_t>();
            d.after_margin_available_qty = j.at("after_margin_available_qty").get<int64_t>();
            d.after_short_available_amount = j.at("after_short_available_amount").get<int64_t>();
            d.after_short_available_qty = j.at("after_short_available_qty").get<int64_t>();
            d.belong_branches = j.at("belong_branches").get<std::vector<std::string>>();
            return Result<SummaryData, ResultError>::Ok(std::move(d));
        }
        catch (const std::exception &ex)
        {
            return Result<SummaryData, ResultError>::Err(ResultError(ex.what()));
        }
    }
} // namespace finance::infrastructure::storage
