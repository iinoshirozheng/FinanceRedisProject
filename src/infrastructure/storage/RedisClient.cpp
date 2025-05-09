#include "RedisClient.h"
#include <sstream>

namespace finance::infrastructure::storage
{
    using finance::domain::ErrorCode;

    template <typename T>
    RedisClient<T>::RedisClient()
        : redisContext_(nullptr)
    {
    }

    template <typename T>
    RedisClient<T>::~RedisClient()
    {
        if (redisContext_)
        {
            disconnect();
        }
    }

    template <typename T>
    Result<void, domain::ErrorResult> RedisClient<T>::connect(const std::string &host, int port)
    {
        redisContext_ = RedisContextPtr(redisConnect(host.c_str(), port));
        if (!redisContext_)
        {
            // Use ErrorCode::RedisContextAllocationError
            return Result<void, domain::ErrorResult>::Err(
                domain::ErrorResult{ErrorCode::RedisContextAllocationError, "Unable to create Redis context"});
        }
        if (redisContext_->err)
        {
            std::string errMsg = redisContext_->errstr;
            redisContext_.reset(); // Close connection
            // Use ErrorCode::RedisConnectionFailed
            return Result<void, domain::ErrorResult>::Err(
                domain::ErrorResult{ErrorCode::RedisConnectionFailed, "Redis connection failed: " + errMsg});
        }

        return Result<void, domain::ErrorResult>::Ok();
    }

    template <typename T>
    Result<void, domain::ErrorResult> RedisClient<T>::disconnect()
    {
        if (!redisContext_)
        {
            // Returning success since disconnection is a no-op if already disconnected
            return Result<void, domain::ErrorResult>::Ok();
        }

        redisContext_.reset();
        return Result<void, domain::ErrorResult>::Ok();
    }

    template <typename T>
    Result<T, domain::ErrorResult> RedisClient<T>::get(const std::string &key)
    {
        if (!redisContext_)
        {
            // Use ErrorCode::RedisConnectionFailed
            return Result<T, domain::ErrorResult>::Err(
                domain::ErrorResult{ErrorCode::RedisConnectionFailed, "Redis is not connected"});
        }

        // Execute GET command
        redisReply *reply = (redisReply *)::redisCommand(redisContext_.get(), "GET %s", key.c_str());
        if (!reply)
        {
            // Use ErrorCode::RedisCommandFailed
            return Result<T, domain::ErrorResult>::Err(
                domain::ErrorResult{ErrorCode::RedisCommandFailed, "GET command failed: no reply"});
        }

        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, freeReplyObject);

        if constexpr (std::is_same_v<T, std::string>)
        {
            if (reply->type == REDIS_REPLY_STRING)
            {
                return Result<T, domain::ErrorResult>::Ok(reply->str);
            }
            if (reply->type == REDIS_REPLY_NIL)
            {
                // Use ErrorCode::RedisKeyNotFound
                return Result<T, domain::ErrorResult>::Err(
                    domain::ErrorResult{ErrorCode::RedisKeyNotFound, "GET key not found: " + key});
            }
        }

        // Use ErrorCode::RedisReplyTypeError for unexpected reply types
        return Result<T, domain::ErrorResult>::Err(
            domain::ErrorResult{ErrorCode::RedisReplyTypeError, "GET error: unexpected reply type"});
    }

    template <typename T>
    Result<void, domain::ErrorResult> RedisClient<T>::set(const std::string &key, const T &value)
    {
        static_assert(std::is_same_v<T, std::string>, "Only std::string is supported for T");

        if (!redisContext_)
        {
            // Use ErrorCode::RedisConnectionFailed
            return Result<void, domain::ErrorResult>::Err(
                domain::ErrorResult{ErrorCode::RedisConnectionFailed, "Redis is not connected"});
        }

        // Execute SET command
        redisReply *reply = (redisReply *)::redisCommand(redisContext_.get(), "SET %s %s", key.c_str(), value.c_str());
        if (!reply)
        {
            // Use ErrorCode::RedisCommandFailed
            return Result<void, domain::ErrorResult>::Err(
                domain::ErrorResult{ErrorCode::RedisCommandFailed, "SET command failed"});
        }

        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, freeReplyObject);

        if (reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK")
        {
            return Result<void, domain::ErrorResult>::Ok();
        }

        // Use ErrorCode::RedisReplyTypeError for unexpected reply types
        return Result<void, domain::ErrorResult>::Err(
            domain::ErrorResult{ErrorCode::RedisReplyTypeError, "SET error: unexpected reply type"});
    }

    template <typename T>
    Result<void, domain::ErrorResult> RedisClient<T>::del(const std::string &key)
    {
        if (!redisContext_)
        {
            return Result<void, domain::ErrorResult>::Err(
                domain::ErrorResult{ErrorCode::RedisConnectionFailed, "Redis is not connected"});
        }

        // Execute DEL command
        redisReply *reply = (redisReply *)::redisCommand(redisContext_.get(), "DEL %s", key.c_str());
        if (!reply)
        {
            return Result<void, domain::ErrorResult>::Err(
                domain::ErrorResult{ErrorCode::RedisCommandFailed, "DEL command failed"});
        }

        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, freeReplyObject);

        if (reply->type == REDIS_REPLY_INTEGER)
        {
            // DEL returns the number of keys that were removed
            // Success even if key doesn't exist (returns 0)
            return Result<void, domain::ErrorResult>::Ok();
        }

        return Result<void, domain::ErrorResult>::Err(
            domain::ErrorResult{ErrorCode::RedisReplyTypeError, "DEL error: unexpected reply type"});
    }

    template <typename T>
    Result<std::vector<std::string>, domain::ErrorResult> RedisClient<T>::keys(const std::string &pattern)
    {
        if (!redisContext_)
        {
            // Use ErrorCode::RedisConnectionFailed
            return Result<std::vector<std::string>, domain::ErrorResult>::Err(
                domain::ErrorResult{ErrorCode::RedisConnectionFailed, "Redis is not connected"});
        }

        // Execute KEYS command
        redisReply *reply = (redisReply *)::redisCommand(redisContext_.get(), "KEYS %s", pattern.c_str());
        if (!reply)
        {
            // Use ErrorCode::RedisCommandFailed
            return Result<std::vector<std::string>, domain::ErrorResult>::Err(
                domain::ErrorResult{ErrorCode::RedisCommandFailed, "Redis KEYS command failed"});
        }

        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, freeReplyObject);

        // Check if reply type is an array
        if (reply->type != REDIS_REPLY_ARRAY)
        {
            // Use ErrorCode::RedisReplyTypeError
            return Result<std::vector<std::string>, domain::ErrorResult>::Err(
                domain::ErrorResult{ErrorCode::RedisReplyTypeError, "Redis KEYS error: unexpected reply type"});
        }

        // Convert keys to std::vector<std::string>
        std::vector<std::string> keys;
        for (size_t i = 0; i < reply->elements; ++i)
        {
            if (reply->element[i]->type == REDIS_REPLY_STRING)
            {
                keys.emplace_back(reply->element[i]->str);
            }
        }

        return Result<std::vector<std::string>, domain::ErrorResult>::Ok(keys);
    }

    template <typename T>
    template <typename R>
    Result<R, domain::ErrorResult> RedisClient<T>::command(const std::string &redisCommand)
    {
        if (!redisContext_)
        {
            return Result<R, domain::ErrorResult>::Err(
                domain::ErrorResult(ErrorCode::RedisConnectionFailed, "Redis 未連線"));
        }

        // 使用 hiredis 發送 Redis 命令
        redisReply *reply = static_cast<redisReply *>(::redisCommand(redisContext_.get(), redisCommand.c_str()));

        if (!reply)
        {
            // 如果 Redis 返回 NULL，表示命令執行失敗
            return Result<R, domain::ErrorResult>::Err(
                domain::ErrorResult(ErrorCode::RedisCommandFailed, "Redis 命令執行失敗：未返回有效的響應"));
        }

        std::unique_ptr<redisReply, decltype(&freeReplyObject)> replyGuard(reply, freeReplyObject);

        // 根據返回結果類型處理命令結果
        switch (reply->type)
        {
        case REDIS_REPLY_ERROR:
            return Result<R, domain::ErrorResult>::Err(
                domain::ErrorResult(ErrorCode::RedisCommandFailed, "Redis 命令錯誤：" + std::string(reply->str)));

        case REDIS_REPLY_STRING:
            return Result<R, domain::ErrorResult>::Ok(static_cast<T>(std::string(reply->str))); // 返回字串類型結果

        case REDIS_REPLY_INTEGER:
            return Result<R, domain::ErrorResult>::Ok(static_cast<T>(reply->integer)); // 返回整數類型結果

        case REDIS_REPLY_NIL:
            return Result<R, domain::ErrorResult>::Err(
                domain::ErrorResult(ErrorCode::RedisKeyNotFound, "Redis 返回 NIL 值（未找到指定的 Key）")); // NIL 表示未找到 Key

        default:
            return Result<R, domain::ErrorResult>::Err(
                domain::ErrorResult(ErrorCode::RedisReplyTypeError, "Redis 返回未知的數據類型"));
        }
    }
}