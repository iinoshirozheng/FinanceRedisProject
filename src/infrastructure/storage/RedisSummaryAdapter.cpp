#include "RedisSummaryAdapter.h"
#include <loguru.hpp>
#include "../config/ConnectionConfigProvider.hpp"
#include "../../domain/Result.hpp"

namespace finance::infrastructure::storage
{
    using finance::domain::ErrorCode;
    using finance::domain::ErrorResult;
    using finance::domain::Result;

    /**
     * @brief 初始化 Redis 連接
     */
    Result<void, ErrorResult> RedisSummaryAdapter::init()
    {
        // 如果 Redis 客戶端已經初始化，直接返回成功
        if (redisClient_)
        {
            return Result<void, ErrorResult>::Ok();
        }

        // Redis 連接初始化
        auto redis = std::make_unique<RedisPlusPlusClient<std::string>>();
        const std::string uri = config::ConnectionConfigProvider::redisUri();

        // Use direct success/failure checks instead of match
        auto connectResult = redis->connect(uri);
        if (connectResult.is_ok())
        {
            redisClient_ = std::move(redis);
            return Result<void, ErrorResult>::Ok();
        }
        else
        {
            const auto &err = connectResult.unwrap_err();
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisConnectionFailed, "Redis 連線失敗: " + err.message});
        }
    }

    /**
     * @brief 序列化並同步資料到 Redis，同時更新本地緩存。
     */
    Result<void, ErrorResult> RedisSummaryAdapter::sync(const SummaryData &data)
    {
        // 如果尚未初始化 Redis，則嘗試初始化
        if (!redisClient_)
        {
            auto initResult = init();
            if (initResult.is_err())
            {
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::RedisConnectionFailed, "Redis 重新連線失敗: " + initResult.unwrap_err().message});
            }
        }

        const std::string key = makeKey(data);

        // 使用命令式風格：序列化 -> 保存到 Redis -> 更新本地緩存
        auto jsonResult = toJson(data);
        if (jsonResult.is_err())
        {
            return Result<void, ErrorResult>::Err(jsonResult.unwrap_err());
        }

        auto setResult = redisClient_->set(key, jsonResult.unwrap());
        if (setResult.is_err())
        {
            return Result<void, ErrorResult>::Err(setResult.unwrap_err());
        }

        // 更新本地緩存
        std::unique_lock lock(cacheMutex_);
        cache_[key] = data;
        return Result<void, ErrorResult>::Ok();
    }

    /**
     * @brief 從本地緩存或 Redis 中取得資料。
     */
    Result<SummaryData, ErrorResult> RedisSummaryAdapter::get(const std::string &key)
    {
        // 如果尚未初始化 Redis，則嘗試初始化
        if (!redisClient_)
        {
            auto initResult = init();
            if (initResult.is_err())
            {
                return Result<SummaryData, ErrorResult>::Err(
                    ErrorResult{ErrorCode::RedisConnectionFailed, "Redis 重新連線失敗: " + initResult.unwrap_err().message});
            }
        }

        // 從本地緩存中查詢資料
        {
            std::shared_lock lock(cacheMutex_);
            if (auto it = cache_.find(key); it != cache_.end())
            {
                return Result<SummaryData, ErrorResult>::Ok(it->second); // 如果快取中有資料，直接返回
            }
        }

        // 使用命令式風格：從 Redis 獲取 -> 反序列化 -> 更新緩存
        auto jsonResult = redisClient_->get(key);
        if (jsonResult.is_err())
        {
            return Result<SummaryData, ErrorResult>::Err(jsonResult.unwrap_err());
        }

        auto dataResult = fromJson(jsonResult.unwrap());
        if (dataResult.is_err())
        {
            return Result<SummaryData, ErrorResult>::Err(dataResult.unwrap_err());
        }

        // 更新緩存
        {
            std::unique_lock lock(cacheMutex_);
            cache_[key] = dataResult.unwrap();
        }

        return dataResult;
    }

    /**
     * @brief 從 Redis 加載所有符合模式的資料到本地緩存。
     */
    Result<void, ErrorResult> RedisSummaryAdapter::loadAll()
    {
        // 如果尚未初始化 Redis，則嘗試初始化
        if (!redisClient_)
        {
            auto initResult = init();
            if (initResult.is_err())
            {
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::RedisConnectionFailed, "Redis 重新連線失敗: " + initResult.unwrap_err().message});
            }
        }

        // 使用 Redis KEYS 指令獲取所有符合模式的 Key
        const std::string pattern = KEY_PREFIX + ":*";
        auto keysRes = redisClient_->keys(pattern);
        if (keysRes.is_err())
        {
            return Result<void, ErrorResult>::Err(keysRes.unwrap_err());
        }

        // 清理本地緩存並加載新資料
        {
            std::unique_lock lock(cacheMutex_);
            cache_.clear();
        }

        for (const auto &key : keysRes.unwrap())
        {
            auto gre = get(key);
            if (gre.is_ok())
            {
                std::unique_lock lock(cacheMutex_);
                cache_[key] = gre.unwrap();
            }
            else
            {
                LOG_F(WARNING, "載入 Key '%s' 從 Redis 失敗: %s.",
                      key.c_str(), gre.unwrap_err().message.c_str());
            }
        }

        LOG_F(INFO, "已從 Redis 載入 %zu 筆 summary 資料。", cache_.size());
        return Result<void, ErrorResult>::Ok();
    }

    // 以下為輔助函數

    /**
     * @brief 將 SummaryData 序列化為 JSON 字串。
     */
    Result<std::string, ErrorResult> RedisSummaryAdapter::toJson(const SummaryData &data)
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
            return Result<std::string, ErrorResult>::Err(ErrorResult{ErrorCode::JsonParseError, ex.what()});
        }
    }

    /**
     * @brief 將 JSON 字串反序列化為 SummaryData。
     */
    Result<SummaryData, ErrorResult> RedisSummaryAdapter::fromJson(const std::string &jsonStr)
    {
        try
        {
            nlohmann::json j = nlohmann::json::parse(jsonStr);
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
            return Result<SummaryData, ErrorResult>::Err(ErrorResult{ErrorCode::JsonParseError, ex.what()});
        }
    }

    /**
     * @brief 將 SummaryData 的 key 序列化為 Redis key
     */
    std::string RedisSummaryAdapter::makeKey(const SummaryData &data)
    {
        return KEY_PREFIX + ":" + data.stock_id + ":" + data.area_center;
    }

    /**
     * @brief 實現 set 方法
     */
    Result<void, ErrorResult> RedisSummaryAdapter::set(const std::string &key)
    {
        // This interface method is not used directly, use sync instead
        return Result<void, ErrorResult>::Err(
            ErrorResult{ErrorCode::InternalError, "Not implemented: Use sync() instead"});
    }

    /**
     * @brief 實現 update 方法
     */
    Result<void, ErrorResult> RedisSummaryAdapter::update(const std::string &key)
    {
        // This interface method is not used directly, use sync instead
        return Result<void, ErrorResult>::Err(
            ErrorResult{ErrorCode::InternalError, "Not implemented: Use sync() instead"});
    }

    /**
     * @brief 實現 remove 方法
     */
    bool RedisSummaryAdapter::remove(const std::string &key)
    {
        // Implementation could delete from cache and Redis if needed
        return false;
    }

} // namespace finance::infrastructure::storage