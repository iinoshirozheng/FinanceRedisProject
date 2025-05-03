#pragma once

#include <string>
#include <string_view>
#include "../../domain/FinanceDataStructure.h"
#include <cctype>
#include <cstring>
#include <nlohmann/json.hpp>
#include <sstream>

namespace finance::utils
{
#define VAL_SIZE(_DATA_) _DATA_, std::strlen(_DATA_)
#define STR_VIEW(_STR_) FinanceUtils::trim_right_view(_STR_)

    // 金融工具函數
    class FinanceUtils
    {
    public:
        // 將後台格式的數字字符串轉換為整數
        // 例如：前導空格的數字，或帶有符號的數字
        static inline int64_t backOfficeToInt(const char *value, const size_t &length)
        {
            // Mapping special characters ('J' to 'R' -> 1 to 9, and '}' -> 0)
            static constexpr char OFFSET = 'I'; // The character offset for 'J' to map to 1

            if (value == nullptr || length == 0) // Check for null pointer or zero length
            {
                return 0;
            }
            int64_t result = 0;

            // Parse the numeric part from the string (excluding the last character)
            for (size_t i = 0; i < length; ++i)
            {
                if ('0' <= value[i] && value[i] <= '9')
                {
                    // Calculate the number "digit by digit"
                    result = result * 10 + (value[i] - '0');
                }
                else if ('J' <= value[i] && value[i] <= 'R')
                {
                    result = result * 10 + (value[i] - OFFSET);
                    break;
                }
                else if (value[i] == '}')
                {
                    result = result * 10;
                    break;
                }
            }

            return -result; // Return the computed negative value.
        }

        // 提取指定長度的字符串，並移除尾部空格
        static inline std::string trim_right(const char *str, size_t length)
        {
            if (str == nullptr)
            {
                return "";
            }

            while (length > 0 && std::isspace(static_cast<unsigned char>(str[length - 1])))
            {
                --length; // 向左推進，直到找到非空白字符
            }
            return std::string(str, length);
        }

        // 提取指定長度的字符串，並移除尾部空格
        static inline std::string_view trim_right_view(const std::string &str)
        {
            auto end = str.find_last_not_of(" \t\n\r");
            if (end == std::string::npos)
            {
                return std::string_view(str.data(), 0);
            }
            return std::string_view(str.data(), end + 1);
        }

        static inline std::string_view trim_right_view(const char *str)
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

        // 從系統標頭確定消息類型
        static inline domain::MessageTransactionType transactionMessageType(const std::string_view &tcode)
        {
            if (tcode == "ELD001")
            {
                return domain::MessageTransactionType::HCRTM01;
            }
            else if (tcode == "ELD002")
            {
                return domain::MessageTransactionType::HCRTM05P;
            }

            return domain::MessageTransactionType::OTHERS;
        }

        // 通用模板函數，用於根據任意數據結構創建唯一鍵
        template <typename T>
        static std::string generateKey(std::string prefix_key, const T &data)
        {
            std::string prefixKey = std::string(prefix_key);

            if constexpr (std::is_same_v<T, domain::SummaryData>)
            {
                return prefixKey + ":" + data.area_center + ":" + data.stock_id;
            }
            else if constexpr (std::is_same_v<T, domain::MessageDataHCRTM01>)
            {
                return prefixKey + ":" + data.area_center + ":" + data.stock_id;
            }
            else if constexpr (std::is_same_v<T, std::string>)
            {
                return prefix_key + ":ALL:" + data; // If it's already a string, return it as is
            }
            else
            {
                static_assert(sizeof(T) == 0, "Unsupported type for generateKey");
                return "";
            }
        }
    };
} // namespace finance::utils
