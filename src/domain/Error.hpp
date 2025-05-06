#pragma once
#include <string>

namespace finance::domain
{

    /**
     * @brief 全域錯誤碼
     */
    enum class ErrorCode
    {
        Ok = 0,
        RedisInitFailed,
        RedisLoadFailed,
        RedisCommandFailed,
        JsonParseError,
        TcpStartFailed,
        InvalidPacket,
        UnknownTransactionCode,
        InternalError
    };

    /**
     * @brief 全域錯誤物件，包含錯誤碼與描述
     */
    struct Error
    {
        ErrorCode code;
        std::string message;

        Error(ErrorCode c, std::string msg) noexcept
            : code(c), message(std::move(msg)) {}
    };

} // namespace finance::domain