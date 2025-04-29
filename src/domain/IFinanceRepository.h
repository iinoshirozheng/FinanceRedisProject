#pragma once

#include <string>
#include <vector>
#include <map>
#include "./FinanceDataStructures.h"

namespace finance
{
    namespace domain
    {
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

        // 提供 Config 配置數據介面
        class IConfigProvider
        {
        public:
            virtual ~IConfigProvider() = default;

            // 獲取配置數據
            // @return 配置數據
            virtual domain::ConfigData getConfig() = 0;

            // 從文件中載入配置
            // @param filePath 配置文件路徑
            // @return 如果載入成功則返回 true
            virtual bool loadFromFile(const std::string &filePath) = 0;

            // 確認是不是沒有 load 資料
            virtual bool empty() = 0;
        };

    } // namespace domain

} // namespace finance