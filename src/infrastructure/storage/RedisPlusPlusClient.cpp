#include "RedisPlusPlusClient.h"
#include <sstream>

using namespace sw::redis; // Redis++ 命名空間
using finance::domain::ErrorCode;
using finance::domain::ErrorResult;
using finance::domain::Result;
using sw::redis::ReplyError;

namespace finance::infrastructure::storage
{
    template <typename T>
    RedisPlusPlusClient<T>::RedisPlusPlusClient()
        : redis_(std::nullopt)
    {
    }

    template <typename T>
    RedisPlusPlusClient<T>::~RedisPlusPlusClient()
    {
        disconnect();
    }

    /**
     * @brief 連接 Redis 服務器
     */
    template <typename T>
    Result<void, ErrorResult> RedisPlusPlusClient<T>::connect(const std::string &host, int port)
    {
        if (redis_)
        {
            // 如果已連接，直接返回成功
            return Result<void, ErrorResult>::Ok();
        }

        try
        {
            // 使用 Redis++ 的連接配置構造 Redis 連接
            ConnectionOptions opts;
            opts.host = host; // Redis 主機地址
            opts.port = port; // Redis 端口
            opts.keep_alive = true;

            redis_ = Redis(opts); // 初始化 Redis 客戶端
        }
        catch (const Error &err)
        {
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisConnectionFailed, "Redis connection failed: " + std::string(err.what())});
        }

        return Result<void, ErrorResult>::Ok();
    }

    /**
     * @brief 連接 Redis 服務器 uri 版本
     */
    template <typename T>
    Result<void, ErrorResult> RedisPlusPlusClient<T>::connect(const std::string &uri)
    {
        if (redis_)
        {
            // 如果已連接，直接返回成功
            return Result<void, ErrorResult>::Ok();
        }

        try
        {
            // 使用 Redis++ 提供的 URI 連接功能
            redis_ = Redis(uri); // 直接構造 Redis 客戶端
        }
        catch (const Error &err)
        {
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisConnectionFailed, "Redis connection failed (URI): " + std::string(err.what())});
        }

        return Result<void, ErrorResult>::Ok();
    }

    /**
     * @brief 斷開 Redis 連接
     */
    template <typename T>
    Result<void, ErrorResult> RedisPlusPlusClient<T>::disconnect()
    {
        if (!redis_)
        {
            return Result<void, ErrorResult>::Ok(); // 如果已斷開，直接返回成功
        }

        redis_.reset(); // 使用 std::optional 的 reset 方法
        return Result<void, ErrorResult>::Ok();
    }

    /**
     * @brief 從 Redis 獲取值
     */
    template <typename T>
    Result<T, ErrorResult> RedisPlusPlusClient<T>::get(const std::string &key)
    {
        if (!redis_)
        {
            return Result<T, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisConnectionFailed, "Redis is not connected"});
        }

        try
        {
            auto val = redis_->get(key);
            if (val)
            {
                return Result<T, ErrorResult>::Ok(*val); // 假設 T 是 std::string，將 *val 返回
            }
            else
            {
                return Result<T, ErrorResult>::Err(
                    ErrorResult{ErrorCode::RedisKeyNotFound, "GET key not found: " + key});
            }
        }
        catch (const Error &err)
        {
            return Result<T, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisCommandFailed, "GET command failed: " + std::string(err.what())});
        }
    }

    /**
     * @brief 設置 Redis 值
     */
    template <typename T>
    Result<void, ErrorResult> RedisPlusPlusClient<T>::set(const std::string &key, const T &value)
    {
        static_assert(std::is_same_v<T, std::string>, "Only std::string is supported for T");

        if (!redis_)
        {
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisConnectionFailed, "Redis is not connected"});
        }

        try
        {
            redis_->set(key, value);
            return Result<void, ErrorResult>::Ok();
        }
        catch (const Error &err)
        {
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisCommandFailed, "SET command failed: " + std::string(err.what())});
        }
    }

    /**
     * @brief 從 Redis 刪除鍵
     */
    template <typename T>
    Result<void, ErrorResult> RedisPlusPlusClient<T>::del(const std::string &key)
    {
        if (!redis_)
        {
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisConnectionFailed, "Redis is not connected"});
        }

        try
        {
            redis_->del(key);
            return Result<void, ErrorResult>::Ok();
        }
        catch (const Error &err)
        {
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisCommandFailed, "DEL command failed: " + std::string(err.what())});
        }
    }

    /**
     * @brief 執行自定義 Redis 命令
     */
    template <typename T>
    template <typename R>
    Result<R, ErrorResult> RedisPlusPlusClient<T>::command(const std::string &redisCommand)
    {
        if (!redis_)
        {
            return Result<R, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisConnectionFailed, "Redis is not connected"});
        }

        try
        {
            auto reply = redis_->command(redisCommand);
            // Note: This is a simplified implementation.
            // Proper implementation would depend on the actual type R and the command executed
            return Result<R, ErrorResult>::Ok(static_cast<R>(reply));
        }
        catch (const Error &err)
        {
            return Result<R, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisCommandFailed, "Command failed: " + std::string(err.what())});
        }
    }

    /**
     * @brief 使用模式查詢所有匹配的鍵
     */
    template <typename T>
    Result<std::vector<std::string>, ErrorResult> RedisPlusPlusClient<T>::keys(const std::string &pattern)
    {
        if (!redis_)
        {
            return Result<std::vector<std::string>, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisConnectionFailed, "Redis is not connected"});
        }

        try
        {
            std::vector<std::string> result;
            redis_->keys(pattern, std::back_inserter(result));
            return Result<std::vector<std::string>, ErrorResult>::Ok(result);
        }
        catch (const Error &err)
        {
            return Result<std::vector<std::string>, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisCommandFailed, "KEYS command failed: " + std::string(err.what())});
        }
    }

    /**
     * @brief 使用 RedisJSON 模塊獲取 JSON 數據
     */
    template <typename T>
    Result<std::string, ErrorResult> RedisPlusPlusClient<T>::getJson(const std::string &key, const std::string &path)
    {
        if (!redis_)
        {
            return Result<std::string, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisConnectionFailed, "Redis is not connected"});
        }

        try
        {
            // Use Redis++ to execute JSON.GET command directly
            std::string command = "JSON.GET " + key + " " + path;
            auto result = redis_->command<std::string>(command.c_str());
            return Result<std::string, ErrorResult>::Ok(result);
        }
        catch (const ReplyError &e)
        {
            // Handle Redis reply errors
            if (std::string(e.what()).find("ERR key not found") != std::string::npos)
            {
                return Result<std::string, ErrorResult>::Err(
                    ErrorResult{ErrorCode::RedisKeyNotFound, "JSON key not found: " + key});
            }
            return Result<std::string, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisReplyTypeError, std::string("JSON.GET error: ") + e.what()});
        }
        catch (const Error &err)
        {
            return Result<std::string, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisCommandFailed, "JSON.GET command failed: " + std::string(err.what())});
        }
    }

    /**
     * @brief 使用 RedisJSON 模塊設置 JSON 數據
     */
    template <typename T>
    Result<void, ErrorResult> RedisPlusPlusClient<T>::setJson(const std::string &key, const std::string &path, const std::string &jsonValue)
    {
        if (!redis_)
        {
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisConnectionFailed, "Redis is not connected"});
        }

        try
        {
            // Use Redis++ to execute JSON.SET command directly
            std::string command = "JSON.SET " + key + " " + path + " " + jsonValue;
            redis_->command<void>(command.c_str());
            return Result<void, ErrorResult>::Ok();
        }
        catch (const Error &err)
        {
            return Result<void, ErrorResult>::Err(
                ErrorResult{ErrorCode::RedisCommandFailed, "JSON.SET command failed: " + std::string(err.what())});
        }
    }
}

// Explicit template instantiation for std::string
template class finance::infrastructure::storage::RedisPlusPlusClient<std::string>;