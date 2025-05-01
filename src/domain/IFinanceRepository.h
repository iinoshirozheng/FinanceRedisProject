#pragma once

#include <string>
#include <vector>
#include <map>
#include "./FinanceDataStructure.h"

namespace finance::domain
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
        virtual bool update(const std::string &key, const Data &data) = 0;

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

        /**
         * @brief Update the company summary for a specific stock
         * @param stockId The stock ID to update the summary for
         * @return true if updated successfully, false otherwise
         */
        virtual bool updateCompanySummary(const std::string &stockId) = 0;
    };

} // namespace finance::domain