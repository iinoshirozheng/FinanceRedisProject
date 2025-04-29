#pragma once

#include <string>
#include <string_view>
#include "../domain/FinanceDataStructures.h"
#include <cctype>

namespace finance
{
    namespace common
    {
#define VAL_SIZE(_DATA_) _DATA_, std::strlen(_DATA_)
#define STR_VIEW(_CHAR_) FinanceUtils::trim_right_view(_CHAR_, sizeof(_CHAR_))
#define BACK_OFFICE_INT(_DATA_) FinanceUtils::backOfficeToInt(_DATA_, sizeof(_DATA_))

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
            static inline void trim_right_cstr(char *string, size_t length)
            {
                // 如果字串是空的，或者長度是 0，直接返回
                if (string == nullptr || length == 0)
                {
                    return;
                }
                // 從字串末尾開始，向左掃描
                char *end = string + length - 1;
                while (end >= string && std::isspace(static_cast<unsigned char>(*end)))
                {
                    --end; // 移動指針到非空白的地方
                }

                // 將最後的有效字元的下一個位置設定為 '\0'
                *(end + 1) = '\0';
            }

            // 提取指定長度的字符串，並移除尾部空格
            static inline std::string_view trim_right_view(const char *string, size_t length)
            {
                if (string == nullptr || length == 0)
                {
                    return "";
                }

                size_t end = length;
                while (end > 0 && std::isspace(static_cast<unsigned char>(string[end - 1])))
                {
                    --end;
                }

                return std::string_view(string, end); // 返回字符串视图
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
            static inline std::string generateKey(const T &data, const std::string &area_center = "", const std::string &stock_id = "", const std::string &broker_id = "")
            {
                using namespace std::string_literals;

                if constexpr (std::is_same_v<T, domain::MessageDataHCRTM01>)
                {
                    std::string area_center = trim_right(VAL_SIZE(data.areaCenter));
                    std::string stock_id = trim_right(VAL_SIZE(data.stockId));
                    return "summary:"s + area_center + ":" + stock_id;
                }
                else if constexpr (std::is_same_v<T, domain::MessageDataHCRTM05P>)
                {
                    std::string broker_id = trim_right(VAL_SIZE(data.brokerId));
                    std::string stock_id = trim_right(VAL_SIZE(data.stockId));
                    return "summary:"s + broker_id + ":" + std::string(data.stockId, sizeof(data.stockId));
                }
                else if constexpr (std::is_same_v<T, domain::SummaryData>)
                {
                    std::string area_center = trim_right(VAL_SIZE(data.areaCenter));
                    std::string stock_id = trim_right(VAL_SIZE(data.stockId));
                    return "summary:"s + area_center + ":" + stock_id;
                }
                else if constexpr (std::is_same_v<T, char *>)
                {
                    std::string stock_id = trim_right(VAL_SIZE(data));
                    return "summary:ALL:"s + stock_id;
                }
                else
                {
                    static_assert(sizeof(T) == 0, "createKeyForMessage : 未支援的數據結構");
                }
            }

            inline long long BackOfficeInt(const std::string &value)
            {
                if (value.empty())
                    return 0;

                auto last_char = value.back();
                if (isdigit(last_char))
                {
                    return std::stoll(value);
                }

                auto part1 = std::stoll(value.substr(0, value.length() - 1));
                auto part2 = last_char - 'A' + 1;
                return -1 * (part1 * 10 + part2);
            }
        };
    } // namespace common

} // namespace finance
