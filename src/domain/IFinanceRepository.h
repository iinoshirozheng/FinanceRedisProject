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
        template <typename Data>
        class IFinanceRepository
        {
        public:
            /**
             * @brief Virtual destructor
             */
            virtual ~IFinanceRepository() = default;

            /**
             * @brief Save a data entity
             * @param data The data entity to save
             * @return true if saved successfully, false otherwise
             */
            virtual bool save(const Data &data) = 0;

            /**
             * @brief Retrieve a data entity by key
             * @param key The key to identify the entity
             * @return The data entity if found, otherwise std::nullopt
             */
            virtual Data *get(const std::string &key) = 0;

            /**
             * @brief Update a data entity
             * @param data The updated data entity
             * @param key The key to identify the entity
             * @return true if updated successfully, false otherwise
             */
            virtual bool update(const Data &data, const std::string &key) = 0;

            /**
             * @brief Delete a data entity
             * @param key The key to identify the entity
             * @return true if deleted successfully, false otherwise
             */
            virtual bool remove(const std::string &key) = 0;

            /**
             * @brief Load all data entities
             * @return Vector of all data entities
             */
            virtual std::vector<Data> loadAll() = 0;

            /**
             * @brief Retrieve all data as a key-value mapping
             * @return A map of all data entities
             */
            virtual std::map<std::string, Data> getAllMapped() = 0;

            /**
             * @brief Retrieve all summarized data by a specific secondary key
             * @param secondaryKey The secondary key (e.g., stock ID or area center)
             * @return A vector of all data entities matching the secondary key
             */
            virtual std::vector<Data> getAllBySecondaryKey(const std::string &secondaryKey) = 0;

            /**
             * @brief Create a search index for efficient queries
             * @return true if the index was created successfully, false otherwise
             */
            virtual bool createIndex() = 0;
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