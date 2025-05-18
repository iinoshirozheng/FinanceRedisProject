#pragma once

#include <string_view>
#include "domain/FinanceDataStructure.hpp"
#include <cstring>
#include "domain/Result.hpp"

// 把後端資料(交易數字碼 交易量)轉成 -INT64
#define CONVERT_BACKOFFICE_INT64(STRUCT_NAME, VAR_NAME)                                                                                             \
    auto RESULT_##VAR_NAME = FinanceUtils::backOfficeToInt(STRUCT_NAME.VAR_NAME, sizeof(STRUCT_NAME.VAR_NAME));                                     \
    if (RESULT_##VAR_NAME.is_err())                                                                                                                 \
    {                                                                                                                                               \
        return Result<void, ErrorResult>::Err(                                                                                                      \
            ErrorResult{ErrorCode::BackOfficeIntParseError, "CONVERT_BACKOFFICE_INT64:backOfficeToInt parse error : " #STRUCT_NAME "." #VAR_NAME}); \
    }                                                                                                                                               \
    int64_t VAR_NAME = RESULT_##VAR_NAME.unwrap();

namespace finance::utils
{
#define VAL_SIZE(_DATA_) _DATA_, std::strlen(_DATA_)
#define STR_VIEW(_STR_) FinanceUtils::trim_right_view(_STR_)

    // 金融工具函數
    class FinanceUtils
    {
    public:
        /**
         * @brief 將後台格式的數字字符串轉換為整數。
         * @details 根據後台協議，成功轉換的結果應為負數或零。最後一個字符表示個位數和潛在的負號信息。
         * 忽略前導和尾部空格，但中間包含空格視為錯誤。遇到其他解析錯誤時，回傳 1。
         * @param value 要轉換的字符串指針
         * @param length 字符串長度
         * @return int64_t 轉換結果，成功時為負數或零，失敗時為 1。
         */
        static inline domain::Result<int64_t, domain::ErrorResult> backOfficeToInt(const char *value, size_t length) noexcept
        {
            // Mapping special characters ('J' to 'R' -> 1 to 9, and '}' -> 0) for the last digit and sign
            static constexpr char OFFSET = 'I'; // The character offset for 'J' to map to 1

            if (value == nullptr || length == 0) // Check for null pointer or zero length
            {
                return domain::Result<int64_t, domain::ErrorResult>::Err(
                    domain::ErrorResult{domain::ErrorCode::BackOfficeIntParseError, "backOfficeToInt: empty input"}); // 空輸入視為成功，結果為 0
            }

            int64_t result = 0;
            bool found_digit = false; // 標記是否已經遇到數字字符

            while (std::isspace(value[length - 1]))
            {
                --length;
            }

            // 遍歷字串
            for (size_t i = 0; i < length; ++i)
            {
                char current_char = value[i];

                if ('0' <= current_char && current_char <= '9')
                {
                    // 是數字字符
                    result = result * 10 + (current_char - '0');
                    found_digit = true; // 標記已找到數字
                }

                else if ('J' <= current_char && current_char <= 'R')
                {
                    return domain::Result<int64_t, domain::ErrorResult>::Ok(-1 * (result * 10 + (current_char - OFFSET)));
                }
                else if (current_char == '}')
                {
                    return domain::Result<int64_t, domain::ErrorResult>::Ok(-result * 10);
                }
                else if (std::isspace(static_cast<unsigned char>(current_char)))
                {
                    if (found_digit)
                        return domain::Result<int64_t, domain::ErrorResult>::Err(
                            domain::ErrorResult{domain::ErrorCode::BackOfficeIntParseError, "backOfficeToInt: space in the middle of the string"});
                }
                else
                {
                    return domain::Result<int64_t, domain::ErrorResult>::Err(
                        domain::ErrorResult{domain::ErrorCode::BackOfficeIntParseError, "backOfficeToInt: invalid character"});
                }
            }

            return domain::Result<int64_t, domain::ErrorResult>::Ok(result);
        }

        // 提取指定長度的字符串，並移除尾部空格 (std::string 版本)
        // 修正測試失敗的問題，改用迴圈檢查尾部空格，邏輯與 const char* 版本一致
        static inline std::string_view trim_right_view(const std::string &str) noexcept
        {
            size_t len = str.length();
            // 從字串尾部向前檢查，直到找到第一個非空白字符或到達字串開頭
            while (len > 0 && std::isspace(static_cast<unsigned char>(str[len - 1])))
            {
                --len; // 如果是空白字符，縮短有效長度
            }
            // 返回一個 string_view，指向原始字串的數據，長度為找到的第一個非空白字符之前的部分
            return std::string_view(str.data(), len);
        }

        // 提取指定長度的字符串，並移除尾部空格 (const char* 版本)
        static inline std::string_view trim_right_view(const char *str) noexcept
        {
            if (!str)
                return std::string_view();
            size_t len = strlen(str);
            while (len > 0 && std::isspace(static_cast<unsigned char>(str[len - 1])))
            {
                --len;
            }
            return std::string_view(str, len);
        }

        // trim_right 函式，返回 std::string，這裡不修改
        static inline std::string trim_right(const char *str, size_t length)
        {
            if (str == nullptr)
            {
                return "";
            }

            while (length > 0 && std::isspace(static_cast<unsigned char>(str[length - 1])))
            {
                --length;
            }
            return std::string(str, length);
        }
    };
} // namespace finance::utils
