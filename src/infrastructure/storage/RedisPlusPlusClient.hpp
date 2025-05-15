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
        inline RedisPlusPlusClient() noexcept
            : redis_(nullptr), poolSize_(10), waitTimeoutMs_(100) {}

        inline ~RedisPlusPlusClient() { disconnect(); }

        // 以 host+port 連線
        inline Result<void, E> connect(const std::string &host, int port, const std::string &password = "")
        {
            if (redis_)
                return Result<void, E>::Ok();
            try
            {
                ConnectionOptions opts;
                opts.host = host;
                opts.port = port;
                opts.password = password;
                opts.keep_alive = true;
                LOG_F(INFO, "Redis connecting to host : %s", host.c_str());
                LOG_F(INFO, "Redis connecting to port : %d", port);

                redis_ = std::make_unique<Redis>(opts);
                LOG_F(INFO, "Redis connected: %s:%d (AUTH %s)", host.c_str(), port, password.c_str());

                return Result<void, E>::Ok();
            }
            catch (const std::exception &ex)
            {
                LOG_F(ERROR, "Redis connect failed: %s", ex.what());
                return Result<void, E>::Err({ErrorCode::RedisConnectionFailed, ex.what()});
            }
        }

        // 以 URL 連線，例如 "tcp://127.0.0.1:6666"
        inline Result<void, E> connect(const std::string &url, const std::string &password = "")
        {
            // 解析 host & port
            std::string u = url;
            if (u.rfind("tcp://", 0) == 0)
                u.erase(0, 6);
            auto pos = u.find(':');
            std::string host = u.substr(0, pos);
            int port = (pos != std::string::npos) ? std::stoi(u.substr(pos + 1)) : 6379;
            return connect(host, port, password);
        }

        inline Result<void, E> disconnect() noexcept
        {
            redis_.reset();
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
        std::unique_ptr<Redis> redis_;
        size_t poolSize_;
        int waitTimeoutMs_;
    };

} // namespace finance::infrastructure::storage
