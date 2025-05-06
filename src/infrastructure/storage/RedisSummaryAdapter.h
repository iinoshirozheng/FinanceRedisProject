// RedisSummaryAdapter.h
#pragma once

#include "../../domain/Result.hpp"
#include "../../domain/FinanceDataStructure.h"
#include "../../utils/FinanceUtils.hpp"
#include "../config/ConnectionConfigProvider.hpp"
#include "RedisClient.hpp"

#include <nlohmann/json.hpp>
#include <shared_mutex>
#include <unordered_map>
#include <string>
#include <vector>

namespace finance::infrastructure::storage
{
    using finance::domain::Result;
    using finance::domain::ResultError;
    using finance::domain::SummaryData;
    using RedisContextPtr = RedisContextPtr;

    /**
     * @brief Redis 上的 SummaryData 儲存庫適配器，支持本機快取與同步到 Redis。
     */
    class RedisSummaryAdapter
    {
    public:
        /**
         * @brief 建構並不立即連線 Redis，需呼叫 init().
         */
        RedisSummaryAdapter() = default;

        ~RedisSummaryAdapter() = default;

        /**
         * @brief 與 Redis 建立連線。
         * @return Result<void, ResultError> 連線結果
         */
        Result<void, ResultError> init();

        /**
         * @brief 序列化並同步資料到 Redis，並更新本地快取。
         * @param data 要同步的 SummaryData
         * @return Result<void, ResultError> 操作結果
         */
        Result<void, ResultError> sync(const SummaryData &data);

        /**
         * @brief 從本地快取或 Redis 讀取資料。
         * @param key 完整的 Redis Key，例如 "summary:AREA:STOCK"
         * @return Result<SummaryData, ResultError> 查詢結果
         */
        Result<SummaryData, ResultError> get(const std::string &key);

        /**
         * @brief 從 Redis 與本地快取刪除資料。
         * @param key 要刪除的 Key
         * @return Result<void, ResultError> 操作結果
         */
        Result<void, ResultError> remove(const std::string &key);

        /**
         * @brief 從 Redis 載入所有符合前綴的資料到本地快取。
         * @return Result<void, ResultError> 載入結果
         */
        Result<void, ResultError> loadAll();

        /**
         * @brief 取得目前所有本地快取的資料。
         * @return unordered_map<string, SummaryData> 快取內容副本
         */
        std::unordered_map<std::string, SummaryData> getAll() const;

    private:
        RedisContextPtr ctx_;
        mutable std::shared_mutex cacheMutex_;
        std::unordered_map<std::string, SummaryData> cache_;

        static constexpr char KEY_PREFIX[] = "summary";

        /**
         * @brief 產生 Redis Key
         */
        static std::string makeKey(const SummaryData &data)
        {
            return utils::FinanceUtils::generateKey(KEY_PREFIX, data);
        }

        /**
         * @brief 以 KEYS 指令取得所有符合 pattern 的 Key。
         */
        Result<std::vector<std::string>, ResultError> getKeys(const std::string &pattern);

        /**
         * @brief 執行 JSON.GET 指令，取得原始 JSON 字串。
         */
        Result<std::string, ResultError> getJson(const std::string &key);

        /**
         * @brief 將 SummaryData 序列化成 JSON。
         */
        static Result<std::string, ResultError> toJson(const SummaryData &data);

        /**
         * @brief 將 JSON 反序列化成 SummaryData。
         */
        static Result<SummaryData, ResultError> fromJson(const std::string &jsonStr);
    };
} // namespace finance::infrastructure::storage
