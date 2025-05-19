

// RedisPlusPlusClient.h
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <sw/redis++/redis++.h>
#include "domain/Result.hpp"
#include <loguru.hpp>

namespace finance::infrastructure::storage
{

    using finance::domain::ErrorCode;
    using finance::domain::ErrorResult;
    using finance::domain::Result;
    using sw::redis::ConnectionOptions;
    using sw::redis::ConnectionPoolOptions;
    using sw::redis::Error;
    using sw::redis::Redis;
    using sw::redis::ReplyError;

    template <typename T, typename E>
    class RedisPlusPlusClient
    {
    public:
        inline RedisPlusPlusClient() noexcept : redis_(nullptr) {}

        inline ~RedisPlusPlusClient() { disconnect(); }

        /**
         * @brief 連接到 Redis 伺服器，可選使用連接池。
         * @param url Redis 連接 URL (例如 "tcp://127.0.0.1:6379")。
         * @param password 認證密碼 (可選)。
         * @param poolSize 連接池大小。如果大於 0，則使用連接池。
         * @param poolTimeoutMs 從連接池獲取連接的超時時間 (毫秒)。
         * @return Result<void, E> 連接結果。
         */
        // Removed noexcept as it can throw
        inline Result<void, E> connect(const std::string &url, const std::string &password = "",
                                       size_t poolSize = 0, int poolTimeoutMs = 0)
        {
            if (redis_) // Check if already connected
                return Result<void, E>::Ok();

            try
            {
                ConnectionOptions connOpts;
                // 解析 URL 並設置 host 和 port
                std::string u = url;
                if (u.rfind("tcp://", 0) == 0)
                    u.erase(0, 6); // 去掉前綴
                if (u.rfind("redis://", 0) == 0)
                    u.erase(0, 8);

                auto pos = u.find(':');
                connOpts.host = (pos == std::string::npos) ? u : u.substr(0, pos);
                connOpts.port = (pos == std::string::npos) ? 6379 : std::stoi(u.substr(pos + 1));
                connOpts.password = password;
                connOpts.keep_alive = true;

                LOG_F(INFO, "Connecting to Redis: host=%s, port=%d (password provided=%s)",
                      connOpts.host.c_str(), connOpts.port, password.empty() ? "None" : password.c_str());

                if (poolSize > 0)
                {
                    ConnectionPoolOptions poolOpts;
                    poolOpts.size = poolSize;
                    poolOpts.wait_timeout = std::chrono::milliseconds(poolTimeoutMs);

                    LOG_F(INFO, "Using Redis connection pool: size=%zu, wait timeout=%d ms", poolSize, poolTimeoutMs);

                    // 使用支持 Connection Pool 的构造函数
                    redis_ = std::make_shared<Redis>(connOpts, poolOpts); // 使用 ConnectionOptions和 ConnectionPoolOptions 初始化 Redis
                }
                else
                {
                    LOG_F(INFO, "Using single Redis connection.");
                    redis_ = std::make_shared<Redis>(connOpts); // 使用单连接初始化 Redis
                }

                // 驗證 PING 命令
                auto ping_res = command<std::string>("PING");
                if (ping_res.is_err())
                {
                    LOG_F(ERROR, "Redis PING failed: %s", ping_res.unwrap_err().message.c_str());
                    redis_.reset();
                    return Result<void, E>::Err(
                        ErrorResult{ErrorCode::RedisConnectionFailed, "PING failed: " + ping_res.unwrap_err().message});
                }
                if (ping_res.unwrap() != "PONG")
                {
                    LOG_F(ERROR, "Redis unexpected PING reply: %s", ping_res.unwrap().c_str());
                    redis_.reset();
                    return Result<void, E>::Err(
                        ErrorResult{ErrorCode::RedisConnectionFailed, "Unexpected PING reply: " + ping_res.unwrap()});
                }

                LOG_F(INFO, "Redis connection successfully verified with PING.");
                return Result<void, E>::Ok();
            }
            catch (const ReplyError &e)
            {
                redis_.reset();
                return Result<void, E>::Err(
                    ErrorResult{ErrorCode::RedisReplyTypeError, "ReplyError during connection: " + std::string(e.what())});
            }
            catch (const Error &e)
            {
                redis_.reset();
                return Result<void, E>::Err(
                    ErrorResult{ErrorCode::RedisCommandFailed, "Error during connection: " + std::string(e.what())});
            }
            catch (const std::exception &ex)
            {
                redis_.reset();
                return Result<void, E>::Err(
                    ErrorResult{ErrorCode::UnexpectedError, "Unexpected exception: " + std::string(ex.what())});
            }
        }

        inline Result<void, E> disconnect() noexcept
        {
            redis_.reset();
            LOG_F(INFO, "Redis client disconnected.");
            return Result<void, E>::Ok();
        }

        inline Result<T, E> get(const std::string &key)
        {
            if (!redis_)
                return Result<T, E>::Err({ErrorCode::RedisConnectionFailed,
                                          "Redis not connected"});
            try
            {
                auto val = redis_->get(key);
                if (!val)
                {
                    return Result<T, E>::Err({ErrorCode::RedisKeyNotFound,
                                              "Key not found: " + key});
                }
                return Result<T, E>::Ok(T(*val));
            }
            catch (const Error &e)
            {
                LOG_F(WARNING, "Redis get error: %s", e.what());
                return Result<T, E>::Err({ErrorCode::RedisCommandFailed, e.what()});
            }
        }

        inline Result<void, E> set(const std::string &key, const T &value)
        {
            if (!redis_)
                return Result<void, E>::Err({ErrorCode::RedisConnectionFailed,
                                             "Redis not connected"});
            try
            {
                redis_->set(key, value);
                return Result<void, E>::Ok();
            }
            catch (const Error &e)
            {
                LOG_F(WARNING, "Redis set error: %s", e.what());
                return Result<void, E>::Err({ErrorCode::RedisCommandFailed, e.what()});
            }
        }

        inline Result<void, E> del(const std::string &key)
        {
            if (!redis_)
                return Result<void, E>::Err({ErrorCode::RedisConnectionFailed,
                                             "Redis not connected"});
            try
            {
                redis_->del(key);
                return Result<void, E>::Ok();
            }
            catch (const Error &e)
            {
                LOG_F(WARNING, "Redis del error: %s", e.what());
                return Result<void, E>::Err({ErrorCode::RedisCommandFailed, e.what()});
            }
        }

        inline Result<std::vector<std::string>, E> keys(const std::string &pattern)
        {
            if (!redis_)
                return Result<std::vector<std::string>, E>::Err(ErrorResult(
                    ErrorCode::RedisConnectionFailed, "Redis not connected"));
            try
            {
                std::vector<std::string> result;
                redis_->keys(pattern, std::back_inserter(result));
                return Result<std::vector<std::string>, E>::Ok(result);
            }
            catch (const Error &e)
            {
                LOG_F(WARNING, "Redis keys error: %s", e.what());
                return Result<std::vector<std::string>, E>::Err(ErrorResult(
                    ErrorCode::RedisCommandFailed, e.what()));
            }
        }

        /**
         * @brief 執行通用的 Redis 命令。
         * @tparam ReplyT 預期的 Redis 回覆類型。
         * @tparam Args 命令參數類型。
         * @param args 命令及其參數。
         * @return Result<ReplyT, E> 命令執行結果。
         */
        template <typename ReplyT, typename... Args>
        Result<ReplyT, E> command(Args &&...args)
        {
            if (!redis_)
                return Result<ReplyT, E>::Err(ErrorResult(ErrorCode::RedisConnectionFailed, "Redis client not connected"));

            try
            {
                // Execute command using the shared Redis connection pool instance
                if constexpr (std::is_same_v<ReplyT, void>)
                {
                    redis_->command<void>(std::forward<Args>(args)...);
                    return Result<void, E>::Ok(); // Success for void return type
                }
                else
                {
                    auto reply = redis_->command<ReplyT>(std::forward<Args>(args)...);
                    return Result<ReplyT, E>::Ok(std::move(reply)); // Success with value
                }
            }
            catch (const ReplyError &e)
            {
                // Handle specific Redis ReplyErrors (e.g., command errors, key not found)
                LOG_F(WARNING, "Redis ReplyError for command: %s", e.what());
                // Map specific messages to known ErrorCodes if possible, otherwise use a generic one
                if (std::string(e.what()).find("ERR key not found") != std::string::npos ||
                    std::string(e.what()).find("no such key") != std::string::npos) // Check common "key not found" messages
                {
                    return Result<ReplyT, E>::Err(
                        ErrorResult{ErrorCode::RedisKeyNotFound, "Redis key not found: " + std::string(e.what())});
                }
                // Other ReplyErrors
                return Result<ReplyT, E>::Err(
                    ErrorResult{ErrorCode::RedisReplyTypeError, "Redis ReplyError: " + std::string(e.what())});
            }
            catch (const Error &e)
            {
                // Handle general Redis++ exceptions (e.g., connection issues, timeouts, pool errors)
                LOG_F(WARNING, "Redis Error for command: %s", e.what());
                return Result<ReplyT, E>::Err(
                    ErrorResult{ErrorCode::RedisCommandFailed, "Redis Command Error: " + std::string(e.what())});
            }
            catch (const std::exception &ex)
            {
                // Catch other potential standard exceptions
                LOG_F(ERROR, "Unexpected exception during Redis command: %s", ex.what());
                return Result<ReplyT, E>::Err(
                    ErrorResult{ErrorCode::UnexpectedError,
                                "意外異常 (Redis 命令): " + std::string(ex.what())});
            }
        }

        inline Result<std::string, E> getJson(const std::string &key,
                                              const std::string &path = "$")
        {
            if (!redis_)
                return Result<std::string, E>::Err(ErrorResult(
                    ErrorCode::RedisConnectionFailed, "Redis not connected"));
            try
            {
                auto str = redis_->command<std::string>("JSON.GET", key, path);
                return Result<std::string, E>::Ok(str);
            }
            catch (const ReplyError &e)
            {
                if (std::string(e.what()).find("ERR key not found") != std::string::npos)
                {
                    return Result<std::string, E>::Err(ErrorResult(
                        ErrorCode::RedisKeyNotFound, "JSON key not found: " + key));
                }
                return Result<std::string, E>::Err(ErrorResult(
                    ErrorCode::RedisReplyTypeError, e.what()));
            }
            catch (const Error &e)
            {
                return Result<std::string, E>::Err(ErrorResult(
                    ErrorCode::RedisCommandFailed, e.what()));
            }
        }

        inline Result<void, E> setJson(const std::string &key,
                                       const std::string &path,
                                       const std::string &jsonValue)
        {
            if (!redis_)
                return Result<void, E>::Err({ErrorCode::RedisConnectionFailed,
                                             "Redis not connected"});
            try
            {
                redis_->command<void>("JSON.SET", key, path, jsonValue);
                return Result<void, E>::Ok();
            }
            catch (const Error &e)
            {
                return Result<void, E>::Err({ErrorCode::RedisCommandFailed, e.what()});
            }
        }

    private:
        std::shared_ptr<Redis> redis_;
    };

} // namespace finance::infrastructure::storage
