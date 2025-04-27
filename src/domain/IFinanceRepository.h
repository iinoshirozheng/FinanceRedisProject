#pragma once

#include "FinanceDataStructures.h"
#include <vector>
#include <string>
#include <optional>
#include <map>
#include <memory>

namespace finance
{
    namespace domain
    {
        // Use SummaryData from FinanceTypes.h

        // 金融數據儲存庫介面
        // 處理儲存和檢索金融摘要數據
        class IFinanceRepository
        {
        public:
            /**
             * @brief Virtual destructor
             */
            virtual ~IFinanceRepository() = default;

            /**
             * @brief Save a summary
             * @param summary The summary data to save
             * @return true if saved successfully, false otherwise
             */
            virtual bool saveSummary(const SummaryData &summary) = 0;

            /**
             * @brief Get a summary by area center
             * @param areaCenter The area center identifier
             * @return The summary data if found, otherwise std::nullopt
             */
            virtual std::optional<SummaryData> getSummary(const std::string &areaCenter) = 0;

            /**
             * @brief Update a summary
             * @param data The updated summary data
             * @param areaCenter The area center identifier
             * @return true if updated successfully, false otherwise
             */
            virtual bool updateSummary(const SummaryData &data, const std::string &areaCenter) = 0;

            /**
             * @brief Delete a summary
             * @param areaCenter The area center identifier
             * @return true if deleted successfully, false otherwise
             */
            virtual bool deleteSummary(const std::string &areaCenter) = 0;

            /**
             * @brief Load all summary data
             * @return Vector of all summary data
             */
            virtual std::vector<SummaryData> loadAllData() = 0;

            // 檢索所有摘要數據，以鍵值映射返回
            // @return 所有摘要數據的映射
            virtual std::map<std::string, SummaryData> getAllSummaries() = 0;

            // 檢索特定股票的所有摘要數據
            // @param stockId 股票ID
            // @return 該股票的所有摘要數據
            virtual std::vector<SummaryData> getAllSummaries(const std::string &stockId) = 0;

            // 創建搜索索引以實現高效查詢
            // @return 如果索引建立成功則返回 true
            virtual bool createSearchIndex() = 0;
        };

        // 提供配置數據的介面
        class IConfigProvider
        {
        public:
            virtual ~IConfigProvider() = default;

            // 獲取配置數據
            // @return 配置數據
            virtual ConfigData getConfig() = 0;

            // 從文件中載入配置
            // @param filePath 配置文件路徑
            // @return 如果載入成功則返回 true
            virtual bool loadFromFile(const std::string &filePath) = 0;
        };

        // 提供區域分支映射數據的介面
        class IAreaBranchRepository
        {
        public:
            virtual ~IAreaBranchRepository() = default;

            // 從文件中載入區域分支映射
            // @param filePath 配置文件路徑
            // @return 如果載入成功則返回 true
            virtual bool loadFromFile(const std::string &filePath) = 0;

            // 獲取指定區域的所有分支 ID
            // @param areaCenter 區域中心代碼
            // @return 分支 ID 向量，如果區域未找到則返回空向量
            virtual std::vector<std::string> getBranchesForArea(const std::string &areaCenter) = 0;

            // 獲取指定分支 ID 的區域中心
            // @param branchId 分支 ID
            // @return 區域中心代碼，如果分支未找到則返回空字符串
            virtual std::string getAreaForBranch(const std::string &branchId) = 0;

            // 獲取所有區域中心代碼
            // @return 所有區域中心代碼的向量
            virtual std::vector<std::string> getAllAreas() = 0;

            // 獲取所有分支 ID
            // @return 所有分支 ID 的向量
            virtual std::vector<std::string> getAllBranches() = 0;
        };

    } // namespace domain
} // namespace finance