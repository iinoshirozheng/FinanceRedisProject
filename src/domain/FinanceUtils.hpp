#pragma once

#include "FinanceDataStructures.h"
#include <string>
#include <map>
#include <cstdint>
#include <boost/algorithm/string.hpp>
#include <cctype>

namespace finance
{
    namespace domain
    {
        // 金融工具函數
        class FinanceUtils
        {
        public:
            // 將後台格式的數字字符串轉換為整數
            // 例如：前導空格的數字，或帶有符號的數字
            static inline int64_t backOfficeToInt(const std::string &value)
            {
                if (value.empty())
                {
                    return 0;
                }

                char lastChar = value.back();

                // 如果最後一個字符是數字，則直接解析
                if (std::isdigit(lastChar))
                {
                    return std::stoll(value);
                }

                // 處理特殊的後台格式
                auto it = backOfficeCodeMap.find(lastChar);
                if (it == backOfficeCodeMap.end())
                {
                    return 0; // 未知字符
                }

                int64_t part1 = std::stoll(value.substr(0, value.length() - 1));
                int64_t part2 = it->second;
                return -1 * (part1 * 10 + part2);
            }

            // 提取指定長度的字符串，並移除尾部空格
            static inline std::string extractString(const std::string &input, size_t start, size_t length)
            {
                if (start >= input.length())
                {
                    return "";
                }

                std::string result = input.substr(start, length);
                boost::algorithm::trim_right(result);
                return result;
            }

            // 從字符數組中提取字符串，並移除尾部空格
            static inline std::string extractString(const char *charArray, size_t size)
            {
                std::string result(charArray, size);
                boost::algorithm::trim_right(result);
                return result;
            }

            // 從固定長度的字符數組中提取字符串
            template <size_t N>
            static inline std::string extractString(const char (&array)[N])
            {
                return std::string(array, strnlen(array, N));
            }

            // 從系統標頭確定消息類型
            static inline MessageType determineMessageType(const std::string &systemHeader)
            {
                // 簡單實現，根據系統頭信息中的某些特徵來確定消息類型
                if (systemHeader.find("ELD001") != std::string::npos)
                {
                    return MessageType::HCRTM01;
                }
                else if (systemHeader.find("ELD002") != std::string::npos)
                {
                    return MessageType::HCRTM05P;
                }

                return MessageType::UNKNOWN;
            }

            // 從票據中確定消息類型
            static inline MessageType determineMessageType(const FinanceBill &bill)
            {
                if (bill.tcode == "ELD001")
                {
                    return MessageType::HCRTM01;
                }
                else if (bill.tcode == "ELD002")
                {
                    return MessageType::HCRTM05P;
                }

                return MessageType::UNKNOWN;
            }

            // 根據 HCRTM01 數據結構創建唯一鍵
            static inline std::string createKeyForHcrtm01(const Hcrtm01Data &data)
            {
                return "summary:" + std::string(data.areaCenter, sizeof(data.areaCenter)) + ":" + std::string(data.stockId, sizeof(data.stockId));
            }

            // 使用 SummaryData 創建 HCRTM01 鍵
            static inline std::string createKeyForHcrtm01(const SummaryData &data)
            {
                return "summary:" + data.areaCenter + ":" + data.stockId;
            }

            // 根據 HCRTM05P 數據結構創建唯一鍵
            static inline std::string createKeyForHcrtm05p(const Hcrtm05pData &data)
            {
                return "summary:" + std::string(data.brokerId, sizeof(data.brokerId)) + ":" + std::string(data.stockId, sizeof(data.stockId));
            }

            // 創建公司級摘要數據的鍵
            static inline std::string createCompanySummaryKey(const std::string &stockId)
            {
                return "summary:ALL:" + stockId;
            }

        private:
            // 後台字母代碼到數字值的映射
            static const std::map<char, int> backOfficeCodeMap;
        };

        // 初始化後台代碼的靜態映射
        inline const std::map<char, int> FinanceUtils::backOfficeCodeMap = {
            {'J', 1},
            {'K', 2},
            {'L', 3},
            {'M', 4},
            {'N', 5},
            {'O', 6},
            {'P', 7},
            {'Q', 8},
            {'R', 9},
            {'}', 0}};

    } // namespace domain
} // namespace finance