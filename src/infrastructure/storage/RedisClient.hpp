#pragma once

#include <hiredis/hiredis.h>
#include <memory>
#include <string>
#include <vector>
#include <sstream>
#include "../../domain/Result.hpp"

namespace finance::infrastructure::storage
{

    struct RedisContextDeleter
    {
        void operator()(redisContext *ctx) const
        {
            if (ctx)
                redisFree(ctx);
        }
    };

    using RedisContextPtr = std::unique_ptr<redisContext, RedisContextDeleter>;

    inline domain::Result<RedisContextPtr, domain::ResultError> connect_redis(const std::string &host, int port)
    {
        redisContext *ctx = redisConnect(host.c_str(), port);
        if (!ctx)
            return domain::Result<RedisContextPtr, domain::ResultError>::Err(domain::ResultError("Unable to allocate redis context"));
        if (ctx->err)
        {
            std::string msg = ctx->errstr;
            redisFree(ctx);
            return domain::Result<RedisContextPtr, domain::ResultError>::Err(domain::ResultError("Redis connection failed: " + msg));
        }
        return domain::Result<RedisContextPtr, domain::ResultError>::Ok(RedisContextPtr(ctx));
    }

    // === SET ===
    inline domain::Result<void, domain::ResultError> redis_set(redisContext *ctx, const std::string &key, const std::string &val)
    {
        redisReply *r = (redisReply *)redisCommand(ctx, "SET %s %s", key.c_str(), val.c_str());
        if (!r)
            return domain::Result<void, domain::ResultError>::Err(domain::ResultError("SET failed"));
        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(r, freeReplyObject);
        if (r->type == REDIS_REPLY_STATUS && std::string(r->str) == "OK")
            return domain::Result<void, domain::ResultError>::Ok();
        return domain::Result<void, domain::ResultError>::Err(domain::ResultError("SET error: unexpected reply"));
    }

    // === GET ===
    inline domain::Result<std::string, domain::ResultError> redis_get(redisContext *ctx, const std::string &key)
    {
        redisReply *r = (redisReply *)redisCommand(ctx, "GET %s", key.c_str());
        if (!r)
            return domain::Result<std::string, domain::ResultError>::Err(domain::ResultError("GET failed"));
        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(r, freeReplyObject);
        if (r->type == REDIS_REPLY_STRING)
            return domain::Result<std::string, domain::ResultError>::Ok(std::string(r->str));
        if (r->type == REDIS_REPLY_NIL)
            return domain::Result<std::string, domain::ResultError>::Err(domain::ResultError("GET key not found"));
        return domain::Result<std::string, domain::ResultError>::Err(domain::ResultError("GET error: unexpected reply"));
    }

    // === DEL ===
    inline domain::Result<void, domain::ResultError> redis_del(redisContext *ctx, const std::string &key)
    {
        redisReply *r = (redisReply *)redisCommand(ctx, "DEL %s", key.c_str());
        if (!r)
            return domain::Result<void, domain::ResultError>::Err(domain::ResultError("DEL failed"));
        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(r, freeReplyObject);
        if (r->type == REDIS_REPLY_INTEGER && r->integer >= 1)
            return domain::Result<void, domain::ResultError>::Ok();
        return domain::Result<void, domain::ResultError>::Err(domain::ResultError("DEL error or key not found"));
    }

    // === INCR ===
    inline domain::Result<int64_t, domain::ResultError> redis_incr(redisContext *ctx, const std::string &key)
    {
        redisReply *r = (redisReply *)redisCommand(ctx, "INCR %s", key.c_str());
        if (!r)
            return domain::Result<int64_t, domain::ResultError>::Err(domain::ResultError("INCR failed"));
        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(r, freeReplyObject);
        if (r->type == REDIS_REPLY_INTEGER)
            return domain::Result<int64_t, domain::ResultError>::Ok(r->integer);
        return domain::Result<int64_t, domain::ResultError>::Err(domain::ResultError("INCR error: unexpected reply"));
    }

    // === HSET ===
    inline domain::Result<void, domain::ResultError> redis_hset(redisContext *ctx, const std::string &key,
                                                                const std::string &field, const std::string &value)
    {
        redisReply *r = (redisReply *)redisCommand(ctx, "HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
        if (!r)
            return domain::Result<void, domain::ResultError>::Err(domain::ResultError("HSET failed"));
        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(r, freeReplyObject);
        if (r->type == REDIS_REPLY_INTEGER)
            return domain::Result<void, domain::ResultError>::Ok();
        return domain::Result<void, domain::ResultError>::Err(domain::ResultError("HSET error: unexpected reply"));
    }

    // === HGET ===
    inline domain::Result<std::string, domain::ResultError> redis_hget(redisContext *ctx, const std::string &key,
                                                                       const std::string &field)
    {
        redisReply *r = (redisReply *)redisCommand(ctx, "HGET %s %s", key.c_str(), field.c_str());
        if (!r)
            return domain::Result<std::string, domain::ResultError>::Err(domain::ResultError("HGET failed"));
        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(r, freeReplyObject);
        if (r->type == REDIS_REPLY_STRING)
            return domain::Result<std::string, domain::ResultError>::Ok(std::string(r->str));
        if (r->type == REDIS_REPLY_NIL)
            return domain::Result<std::string, domain::ResultError>::Err(domain::ResultError("HGET field not found"));
        return domain::Result<std::string, domain::ResultError>::Err(domain::ResultError("HGET error: unexpected reply"));
    }

    // === LPUSH ===
    inline domain::Result<int64_t, domain::ResultError> redis_lpush(redisContext *ctx, const std::string &key, const std::string &value)
    {
        redisReply *r = (redisReply *)redisCommand(ctx, "LPUSH %s %s", key.c_str(), value.c_str());
        if (!r)
            return domain::Result<int64_t, domain::ResultError>::Err(domain::ResultError("LPUSH failed"));
        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(r, freeReplyObject);
        if (r->type == REDIS_REPLY_INTEGER)
            return domain::Result<int64_t, domain::ResultError>::Ok(r->integer);
        return domain::Result<int64_t, domain::ResultError>::Err(domain::ResultError("LPUSH error: unexpected reply"));
    }

    // === LRANGE ===
    inline domain::Result<std::vector<std::string>, domain::ResultError> redis_lrange(redisContext *ctx,
                                                                                      const std::string &key, int start, int stop)
    {
        redisReply *r = (redisReply *)redisCommand(ctx, "LRANGE %s %d %d", key.c_str(), start, stop);
        if (!r)
            return domain::Result<std::vector<std::string>, domain::ResultError>::Err(domain::ResultError("LRANGE failed"));
        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(r, freeReplyObject);
        if (r->type != REDIS_REPLY_ARRAY)
            return domain::Result<std::vector<std::string>, domain::ResultError>::Err(domain::ResultError("LRANGE error: unexpected reply"));

        std::vector<std::string> result;
        for (size_t i = 0; i < r->elements; ++i)
        {
            if (r->element[i]->type == REDIS_REPLY_STRING)
                result.emplace_back(r->element[i]->str);
        }
        return domain::Result<std::vector<std::string>, domain::ResultError>::Ok(result);
    }

} // namespace finance::infrastructure::storage
