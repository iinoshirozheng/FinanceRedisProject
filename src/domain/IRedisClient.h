#pragma once

#include "Result.hpp"
#include <string>
#include <vector>

namespace finance::domain
{
    template <typename T, typename E>
    class IRedisClient
    {
    public:
        virtual ~IRedisClient() = default;

        // Establish a connection to the Redis server
        virtual Result<void, E> connect(const std::string &host, int port) = 0;

        // Disconnect from the Redis server
        virtual Result<void, E> disconnect() = 0;

        // Basic Redis commands
        virtual Result<T, E> get(const std::string &key) = 0;
        virtual Result<void, E> set(const std::string &key, const T &value) = 0;
        virtual Result<void, E> del(const std::string &key) = 0;
        virtual Result<std::vector<std::string>, E> keys(const std::string &pattern) = 0;
    };
}