#pragma once

#include "../../domain/IRedisClient.h"
#include <sw/redis++/redis++.h>
#include <memory>
#include <string>
#include <vector>

namespace finance::infrastructure::storage
{
    using finance::domain::ErrorResult;
    using finance::domain::Result;

    template <typename T>
    class RedisPlusPlusClient : public finance::domain::IRedisClient<T, ErrorResult>
    {
    public:
        RedisPlusPlusClient();
        ~RedisPlusPlusClient() override;

        Result<void, ErrorResult> connect(const std::string &host, int port) override;
        Result<void, ErrorResult> connect(const std::string &uri);
        Result<void, ErrorResult> disconnect() override;

        Result<T, ErrorResult> get(const std::string &key) override;
        Result<void, ErrorResult> set(const std::string &key, const T &value) override;
        Result<void, ErrorResult> del(const std::string &key) override;
        Result<std::vector<std::string>, ErrorResult> keys(const std::string &pattern) override;

        template <typename R>
        Result<R, ErrorResult> command(const std::string &redisCommand);

        // RedisJSON module commands
        Result<std::string, ErrorResult> getJson(const std::string &key, const std::string &path = ".");
        Result<void, ErrorResult> setJson(const std::string &key, const std::string &path, const std::string &jsonValue);

    private:
        std::optional<sw::redis::Redis> redis_; // Redis++ 客戶端
    };
}