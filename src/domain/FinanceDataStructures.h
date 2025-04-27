#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <ctime>

namespace finance
{
    namespace domain
    {
        // Constants for data lengths and buffer sizes
        struct FinanceConstants
        {
            static constexpr size_t MAX_BUF_LEN = 409800;
            static constexpr size_t AMOUNT_LEN = 11;
            static constexpr size_t QTY_LEN = 6;
            static constexpr size_t STOCK_ID_LEN = 6;
            static constexpr size_t AREA_CENTER_LEN = 3;
            static constexpr size_t BROKER_ID_LEN = 4;
        };

        // Summary data for margin trading
        struct SummaryData
        {
            std::string stockId;
            std::string areaCenter;
            int64_t marginBuy{0};
            int64_t shortSell{0};
            int64_t stockLendingAmount{0};
            int64_t stockLendingRemaining{0};
            int64_t marginBuyClosingAmount{0};
            int64_t shortSellClosingAmount{0};
            std::string marginBuyRateStatus;
            int64_t marginBuyRate{0};
            std::string shortSellRateStatus;
            int64_t shortSellRate{0};
            std::vector<std::string> belongBranches;

            // Additional fields for backward compatibility
            int64_t marginAvailableAmount{0};
            int64_t marginAvailableQty{0};
            int64_t shortAvailableAmount{0};
            int64_t shortAvailableQty{0};
            int64_t afterMarginAvailableAmount{0};
            int64_t afterMarginAvailableQty{0};
            int64_t afterShortAvailableAmount{0};
            int64_t afterShortAvailableQty{0};
        };

        // HCRTM01 data structure
        struct Hcrtm01Data
        {
            char stockId[FinanceConstants::STOCK_ID_LEN]{};
            char areaCenter[FinanceConstants::AREA_CENTER_LEN]{};
            int64_t marginBuy{0};
            int64_t shortSell{0};
            int64_t stockLendingAmount{0};
            int64_t stockLendingRemaining{0};
            int64_t marginBuyClosingAmount{0};
            int64_t shortSellClosingAmount{0};

            // Original fields for compatibility
            char brokerId[FinanceConstants::BROKER_ID_LEN]{};
            char marginAmount[FinanceConstants::AMOUNT_LEN]{};
            char marginBuyOrderAmount[FinanceConstants::AMOUNT_LEN]{};
            char marginSellMatchAmount[FinanceConstants::AMOUNT_LEN]{};
            char marginQty[FinanceConstants::QTY_LEN]{};
            char marginBuyOrderQty[FinanceConstants::QTY_LEN]{};
            char marginSellMatchQty[FinanceConstants::QTY_LEN]{};
            char shortAmount[FinanceConstants::AMOUNT_LEN]{};
            char shortSellOrderAmount[FinanceConstants::AMOUNT_LEN]{};
            char shortBuyMatchAmount[FinanceConstants::AMOUNT_LEN]{};
            char shortQty[FinanceConstants::QTY_LEN]{};
            char shortSellOrderQty[FinanceConstants::QTY_LEN]{};
            char shortBuyMatchQty[FinanceConstants::QTY_LEN]{};
        };

        // HCRTM05P data structure
        struct Hcrtm05pData
        {
            char stockId[FinanceConstants::STOCK_ID_LEN]{};
            char brokerId[FinanceConstants::BROKER_ID_LEN]{};
            std::string marginBuyRateStatus;
            int64_t marginBuyRate{0};
            std::string shortSellRateStatus;
            int64_t shortSellRate{0};

            // Original fields for compatibility
            char dummy[1]{};
            char account[7]{};
            char financingCompany[4]{};
            char marginBuyMatchQty[FinanceConstants::QTY_LEN]{};
            char shortSellMatchQty[FinanceConstants::QTY_LEN]{};
            char dayTradeMarginMatchQty[FinanceConstants::QTY_LEN]{};
            char dayTradeShortMatchQty[FinanceConstants::QTY_LEN]{};
            char marginBuyOffsetQty[FinanceConstants::QTY_LEN]{};
            char shortSellOffsetQty[FinanceConstants::QTY_LEN]{};
        };

        // Financial bill data structure
        struct FinanceBill
        {
            std::string billId;
            double amount{0.0};
            std::time_t timestamp{0};
            std::string systemHeader;
            std::string data;
            std::string tcode;

            // Original fields for compatibility
            char pcode[4]{};
            char srcid[3]{};
            char timestampStr[26]{};
            char filler[61]{};
        };

        // Financial message types
        enum class MessageType
        {
            HCRTM01,
            HCRTM05P,
            UNKNOWN
        };

        // Configuration data
        struct ConfigData
        {
            std::string redisUrl{"tcp://127.0.0.1:6479"};
            int serverPort{9516};
            bool initializeIndices{false};
        };

    } // namespace domain
} // namespace finance