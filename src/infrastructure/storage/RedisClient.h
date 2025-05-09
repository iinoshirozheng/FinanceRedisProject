#pragma once

#include <hiredis/hiredis.h>
#include "../../domain/IRedisClient.h"
#include <memory>
#include <string>
#include <vector>

namespace finance::infrastructure::storage
{
    using finance::domain::ErrorResult;
    using finance::domain::Result;

    template <typename T>
    class RedisClient : public finance::domain::IRedisClient<T, domain::ErrorResult>
    {
    public:
        // Constructor
        RedisClient();
        ~RedisClient() override;

        // Interface implementation
        Result<void, domain::ErrorResult> connect(const std::string &host, int port) override;
        Result<void, domain::ErrorResult> disconnect() override;

        Result<T, domain::ErrorResult> get(const std::string &key) override;
        Result<void, domain::ErrorResult> set(const std::string &key, const T &value) override;
        Result<void, domain::ErrorResult> del(const std::string &key) override;
        Result<std::vector<std::string>, domain::ErrorResult> keys(const std::string &pattern) override;

        template <typename R>
        Result<R, domain::ErrorResult> command(const std::string &redisCommand);

    private:
        struct RedisContextDeleter
        {
            void operator()(redisContext *ctx) const
            {
                if (ctx)
                    redisFree(ctx);
            }
        };

        using RedisContextPtr = std::unique_ptr<redisContext, RedisContextDeleter>;
        RedisContextPtr redisContext_;
    };
}