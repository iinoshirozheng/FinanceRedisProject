#pragma once

#include "../../domain/Result.hpp"
#include "../../domain/FinanceDataStructure.h"
#include "../../utils/FinanceUtils.hpp"
#include "../config/ConnectionConfigProvider.hpp"
#include "RedisPlusPlusClient.hpp"
#include "../../domain/IFinanceRepository.h"
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <nlohmann/json.hpp>
#include <loguru.hpp>
#include <optional>
#include <vector>

namespace finance::infrastructure::storage
{
    using finance::domain::ErrorCode;
    using finance::domain::ErrorResult;
    using finance::domain::Result;
    using finance::domain::SummaryData;

    /**
     * @brief Redis 上 SummaryData 資料存儲的適配器，提供本地緩存與與 Redis 的同步功能。
     */
    class RedisSummaryAdapter : public finance::domain::IFinanceRepository<SummaryData, ErrorResult>
    {
    public:
        /**
         * @brief 構造函數但不立即連接 Redis，需要調用 init() 來初始化連線。
         */
        RedisSummaryAdapter() = default;

        ~RedisSummaryAdapter() = default;

        /**
         * @brief 初始化 Redis 連接。
         * @return Result<void> 初始化結果
         */
        Result<void, ErrorResult> init() override;

        /**
         * @brief 序列化並同步資料到 Redis，同時更新本地緩存。
         * @param data 要同步的 SummaryData 資料
         * @return Result<void> 操作結果
         */
        Result<void, ErrorResult> sync(const SummaryData &data) override;

        /**
         * @brief 從本地緩存或 Redis 中讀取資料。
         * @param key 完整的 Redis Key，例如 "summary:AREA:STOCK"
         * @return Result<SummaryData> 查詢結果
         */
        Result<SummaryData, ErrorResult> get(const std::string &key) override;

        /**
         * @brief 從 Redis 加載所有符合模式的資料到本地緩存。
         * @return Result<void> 加載結果
         */
        Result<void, ErrorResult> loadAll() override;

        /**
         * @brief 實現 IFinanceRepository 接口的 set 方法
         * @param key 要設置的 key
         * @return Result<void> 操作結果
         */
        Result<void, ErrorResult> set(const std::string &key) override;

        /**
         * @brief 實現 IFinanceRepository 接口的 update 方法
         * @param key 要更新的 key
         * @return Result<void> 操作結果
         */
        Result<void, ErrorResult> update(const std::string &key) override;

        /**
         * @brief 實現 IFinanceRepository 接口的 remove 方法
         * @param key 要移除的 key
         * @return bool 是否成功移除
         */
        bool remove(const std::string &key) override;

        Result<void, ErrorResult> updateCompanySummary(const std::string &stock_id);

    private:
        std::unique_ptr<RedisPlusPlusClient<SummaryData, ErrorResult>> redisClient_; // Redis 客戶端
        std::unordered_map<std::string, SummaryData> summaryCacheData_;              // 本地緩存

        /**
         * @brief 更新本地緩存，加上鎖
         * @param key 要緩存的鍵值
         * @param data 要緩存的資料
         * @return Result<void, ErrorResult> 緩存更新結果
         */
        Result<void, ErrorResult> setCacheData(const std::string &key, SummaryData data);

        /**
         * @brief 生成 Redis Key。
         */
        std::string makeKey(const SummaryData &data);

        /**
         * @brief 將 SummaryData 序列化為 JSON 字串。
         */
        Result<std::string, ErrorResult> summaryDataToJson(const SummaryData &data);

        /**
         * @brief 將 JSON 反序列化為 SummaryData。
         */
        Result<SummaryData, ErrorResult> jsonToSummaryData(const std::string &jsonStr);

        std::optional<SummaryData> getCachedData(const std::string &key) const;

        /**
         * @brief 加載並快取多個 key 的數據
         * @param keys 要加載的 key 列表
         * @return Result<void> 操作結果
         */
        Result<void, ErrorResult> loadAndCacheKeysData(const std::vector<std::string> &keys);
    };
} // namespace finance::infrastructure::storage