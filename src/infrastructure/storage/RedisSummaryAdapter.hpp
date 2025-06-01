#pragma once

#include "domain/Result.hpp"
#include "domain/FinanceDataStructure.hpp"
#include "utils/FinanceUtils.hpp"
#include "infrastructure/config/ConnectionConfigProvider.hpp"
#include "infrastructure/config/AreaBranchProvider.hpp"
#include "RedisPlusPlusClient.hpp"
#include "domain/IFinanceRepository.hpp"
#include "infrastructure/tasks/RedisTask.hpp"
#include "infrastructure/tasks/RedisWorker.hpp"
#include <string>
#include <unordered_map>
#include <shared_mutex>
#include <nlohmann/json.hpp>
#include <loguru.hpp>
#include <optional>
#include <vector>
#include <future>
#include <functional>

namespace finance::infrastructure::storage
{
    using finance::domain::ErrorCode;
    using finance::domain::ErrorResult;
    using finance::domain::Result;
    using finance::domain::SummaryData;
    using finance::infrastructure::tasks::RedisOperationType;
    using finance::infrastructure::tasks::RedisTask;

    /**
     * @brief Redis 上 SummaryData 資料存儲的適配器，提供本地緩存與與 Redis 的同步功能。
     */
    class RedisSummaryAdapter : public finance::domain::IFinanceRepository<SummaryData, ErrorResult>
    {
    public:
        using TaskSubmitter = std::function<std::future<Result<void, ErrorResult>>(RedisTask)>;

        /**
         * @brief 構造函數但不立即連接 Redis，需要調用 init() 來初始化連線。
         */
        RedisSummaryAdapter(TaskSubmitter submitter = nullptr) noexcept
            : task_submitter_(std::move(submitter)) {}

        ~RedisSummaryAdapter() noexcept = default;

        /**
         * @brief 初始化 Redis 連接。
         * @return Result<void> 初始化結果
         */
        Result<void, ErrorResult> init() override
        {
            // 此處不涉及 summaryCacheData_ 的並行存取，因為通常在單一執行緒中初始化
            if (redisClient_)
                return Result<void, ErrorResult>::Ok();

            auto client = std::make_unique<RedisPlusPlusClient<SummaryData, ErrorResult>>();
            std::string uri = config::ConnectionConfigProvider::redisUri();
            std::string password = config::ConnectionConfigProvider::redisPassword();
            return client->connect(uri, password)
                .and_then([&]
                          {
                    this->redisClient_ = std::move(client);
                    return Result<void, ErrorResult>::Ok(); })
                .and_then([this]
                          {
                    if(initRedisSearchIndex_)
                        return ensureIndex();

                    return Result<void, ErrorResult>::Ok(); })
                .map_err([](const ErrorResult &e)
                         { return ErrorResult{e.code, "Redis 連線失敗: " + e.message}; });
        }

        /**
         * @brief 確保 Redis 上 SummaryData 的 Redisearch 索引存在。
         * @details 如果索引不存在則建立，如果已存在則先刪除後重建 (模擬原始碼行為)。
         * @return Result<void, ErrorResult> 操作結果
         */
        Result<void, ErrorResult> setRedisSearchIndex(bool ensureIndex)
        {
            initRedisSearchIndex_ = ensureIndex;
            return Result<void, ErrorResult>::Ok();
        }

        /**
         * @brief 確保 Redis 上 SummaryData 的 Redisearch 索引存在。
         * @details 如果索引不存在則建立，如果已存在則先刪除後重建 (模擬原始碼行為)。
         * @return Result<void, ErrorResult> 操作結果
         */
        Result<void, ErrorResult> ensureIndex()
        {
            if (!redisClient_)
            {
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::RedisConnectionFailed, "Redis 未正確連線，無法建立索引"});
            }

            const std::string index_name = "outputIdx";
            const std::string key_prefix = "summary:"; // 與 sync 方法中的 key 前綴一致

            // Use Result chain for better error handling than raw try-catch on command calls
            auto create_res = redisClient_->command<void>(
                "FT.CREATE",
                index_name,
                "ON", "JSON",              // 索引類型
                "PREFIX", "1", key_prefix, // 索引前綴
                "SCHEMA",                  // 定義索引欄位
                "$.stock_id", "AS", "stock_id", "TEXT",
                "$.area_center", "AS", "area_center", "TEXT",
                "$.belong_branches.*", "AS", "branches", "TAG" // belong_branches 是陣列，用 .* 和 TAG 建立索引
            );

            if (create_res.is_ok())
            {
                LOG_F(INFO, "Redisearch 索引 '%s' 建立成功。", index_name.c_str());
                return Result<void, ErrorResult>::Ok();
            }

            // If create failed, check if it's an "already exists" error
            const auto &err = create_res.unwrap_err();
            // Check for specific Redis error indicating index already exists
            // Redisearch returns ReplyError for command errors. We need to check the error message.
            // ErrorCode::RedisReplyTypeError is used by RedisPlusPlusClient for ReplyError.
            if (err.code == ErrorCode::RedisReplyTypeError &&
                err.message.find("Index already exists") != std::string::npos)
            {
                LOG_F(WARNING, "Redisearch 索引 '%s' 已存在，嘗試刪除後重建。", index_name.c_str());
                // Index already exists, try dropping it
                auto drop_res = redisClient_->command<void>("FT.DROP", index_name);
                if (drop_res.is_err())
                {
                    // Drop failed, log error and return
                    return Result<void, ErrorResult>::Err(
                        ErrorResult{ErrorCode::RedisCommandFailed, // Use CommandFailed as DROP is a command
                                    "刪除現有 Redisearch 索引 '" + index_name + "' 失敗: " + drop_res.unwrap_err().message});
                }

                // Drop successful, retry creating the index
                auto recreate_res = redisClient_->command<void>(
                    "FT.CREATE",
                    index_name,
                    "ON", "JSON",
                    "PREFIX", "1", key_prefix,
                    "SCHEMA",
                    "$.stock_id", "AS", "stock_id", "TEXT",
                    "$.area_center", "AS", "area_center", "TEXT",
                    "$.belong_branches.*", "AS", "branches", "TAG");

                if (recreate_res.is_ok())
                {
                    LOG_F(INFO, "Redisearch 索引 '%s' 刪除後重建成功。", index_name.c_str());
                    return Result<void, ErrorResult>::Ok();
                }
                else
                {
                    // Recreate failed
                    return Result<void, ErrorResult>::Err(
                        ErrorResult{ErrorCode::RedisCommandFailed, // Use CommandFailed
                                    "重建 Redisearch 索引 '" + index_name + "' 失敗: " + recreate_res.unwrap_err().message});
                }
            }
            else
            {
                // Other index creation failed error
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::RedisCommandFailed, // Use CommandFailed
                                "建立 Redisearch 索引 '" + index_name + "' 失敗: " + err.message});
            }
        }

        /**
         * @brief 序列化並同步資料到 Redis，同時更新本地緩存。
         * @param key 要同步的 key
         * @param data 要同步的 SummaryData 資料
         * @return Result<void> 操作結果
         */
        Result<void, ErrorResult> sync(const std::string &key, const SummaryData *data) override
        {
            LOG_F(WARNING, "RedisSummaryAdapter: Synchronous SYNC called for key %s. Consider using async version.", key.c_str());

            if (!redisClient_)
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::RedisConnectionFailed, "Redis 未正確連線"});

            if (data == nullptr)
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::UnexpectedError, "RedisSummaryAdapter:summary_data = nullptr"});

            // Update local cache with exclusive lock
            {
                std::unique_lock<std::shared_mutex> lock(cacheMutex_);
                summaryCacheData_[key] = *data;
            }

            // Persist to Redis without holding the lock
            return summaryDataToJson(data)
                .and_then([this, &key](const std::string &j)
                          { return redisClient_->setJson(key, "$", j); })
                .map_err([&](const ErrorResult &e)
                         { return ErrorResult{e.code, "Sync 失敗: " + e.message}; });
        }

        /**
         * @brief 從本地緩存或 Redis 中讀取資料。
         * @param key 完整的 Redis Key，例如 "summary:AREA:STOCK"
         * @return Result<SummaryData*> 查詢結果 (返回指向快取中物件的指標)
         */
        // 於 RedisSummaryAdapter.hpp 內
        Result<finance::domain::SummaryData *, finance::domain::ErrorResult> getData(const std::string &key) override
        {
            // 首先嘗試使用共享鎖進行讀取
            {
                std::shared_lock<std::shared_mutex> read_lock(cacheMutex_);
                auto it = summaryCacheData_.find(key);
                if (it != summaryCacheData_.end())
                {
                    return Result<finance::domain::SummaryData *, finance::domain::ErrorResult>::Ok(&(it->second));
                }
            } // read_lock 在此釋放

            // 若未找到，則取得獨占鎖以進行寫入
            std::unique_lock<std::shared_mutex> write_lock(cacheMutex_);

            auto [it, inserted] = summaryCacheData_.emplace(key, finance::domain::SummaryData{});
            return Result<finance::domain::SummaryData *, finance::domain::ErrorResult>::Ok(&(it->second));
        }

        /**
         * @brief 從 Redis 加載所有符合模式的資料到本地緩存。
         * @return Result<void> 加載結果
         */
        Result<void, ErrorResult> loadAll() override
        {
            if (!redisClient_)
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::RedisConnectionFailed, "Redis 未正確連線"});

            const auto pattern = "summary:*";
            // Redis 的 keys 操作本身不直接影響 summaryCacheData_
            return redisClient_->keys(pattern)
                .and_then([this](const std::vector<std::string> &keys)
                          {
                              // loadAndCacheKeysData 會修改 summaryCacheData_，所以需要在其外部或內部加獨佔鎖
                              return this->loadAndCacheKeysData(keys); // 傳遞 this 指標
                          })
                .map_err([](const ErrorResult &e)
                         { return ErrorResult{e.code, "LoadAll 操作失敗: " + e.message}; });
        }

        /**
         * @brief 實現 IFinanceRepository 接口的 set 方法 (通常用於更新或插入快取)
         * @param key 要設置的 key
         * @param data 要寫入的 SummaryData 資料 (傳值，避免 data 的生命週期問題)
         * @return Result<void> 操作結果
         */
        Result<void, ErrorResult> setData(const std::string &key, const SummaryData &data) override
        {
            std::unique_lock<std::shared_mutex> lock(cacheMutex_);
            summaryCacheData_[key] = data;
            return Result<void, ErrorResult>::Ok();
        }

        /**
         * @brief 實現 IFinanceRepository 接口的 update 方法 (計算總公司資料並同步到 Redis)
         * @param stock_id 要更新的股票 ID
         * @return Result<void> 操作結果
         */
        Result<void, ErrorResult> update(const std::string &stock_id) override
        {
            LOG_F(WARNING, "RedisSummaryAdapter: Synchronous UPDATE called for stock_id %s. Consider using async version.", stock_id.c_str());

            if (!redisClient_)
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::RedisConnectionFailed, "Redis 未正確連線"});

            SummaryData company_summary;
            company_summary.stock_id = stock_id;
            company_summary.area_center = "ALL";
            company_summary.belong_branches = config::AreaBranchProvider::getAllBranches();

            // Use shared lock for reading cache data
            {
                std::shared_lock<std::shared_mutex> read_lock(cacheMutex_);
                for (const std::string &officeId : config::AreaBranchProvider::getBackofficeIds())
                {
                    const std::string key = "summary:" + officeId + ":" + stock_id;
                    auto it = summaryCacheData_.find(key);
                    if (it != summaryCacheData_.end())
                    {
                        const auto &area_summary_data = it->second;
                        company_summary.margin_available_amount += area_summary_data.margin_available_amount;
                        company_summary.margin_available_qty += area_summary_data.margin_available_qty;
                        company_summary.short_available_amount += area_summary_data.short_available_amount;
                        company_summary.short_available_qty += area_summary_data.short_available_qty;
                        company_summary.after_margin_available_amount += area_summary_data.after_margin_available_amount;
                        company_summary.after_margin_available_qty += area_summary_data.after_margin_available_qty;
                        company_summary.after_short_available_amount += area_summary_data.after_short_available_amount;
                        company_summary.after_short_available_qty += area_summary_data.after_short_available_qty;
                    }
                }
            }

            auto all_key = "summary:ALL:" + stock_id;
            return sync(all_key, &company_summary);
        }

        /**
         * @brief 實現 IFinanceRepository 接口的 remove 方法
         * @param key 要移除的 key
         * @return bool 是否成功移除
         */
        bool remove(const std::string &key) override
        {
            if (!redisClient_)
                return false;

            // Redis 的 del 操作通常是原子的
            auto res = redisClient_->del(key);
            if (res.is_ok())
            {
                std::unique_lock<std::shared_mutex> lock(cacheMutex_);
                summaryCacheData_.erase(key);
                return true;
            }
            return false;
        }

        /**
         * @brief 異步同步數據到 Redis
         * @param key 鍵值，用於標識數據實體
         * @param data 要同步的數據
         * @return 異步操作結果的 future
         */
        std::future<Result<void, ErrorResult>> sync_async(const std::string &key, const SummaryData &data_to_sync) override
        {
            if (!task_submitter_)
            {
                std::promise<Result<void, ErrorResult>> err_promise;
                err_promise.set_value(Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InternalError, "Task submitter not initialized in RedisSummaryAdapter"}));
                return err_promise.get_future();
            }

            LOG_F(INFO, "RedisSummaryAdapter: Queuing async SYNC task for key %s", key.c_str());
            return task_submitter_(
                RedisTask(
                    RedisOperationType::SYNC_SUMMARY_DATA,
                    key,
                    data_to_sync,
                    nullptr));
        }

        /**
         * @brief 異步更新數據實體
         * @param key 鍵值，用於標識數據實體
         * @return 異步操作結果的 future
         */
        std::future<Result<void, ErrorResult>> update_async(const std::string &stock_id) override
        {
            if (!task_submitter_)
            {
                std::promise<Result<void, ErrorResult>> err_promise;
                err_promise.set_value(Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::InternalError, "Task submitter not initialized"}));
                return err_promise.get_future();
            }

            LOG_F(INFO, "RedisSummaryAdapter: Queuing async UPDATE task for stock_id %s", stock_id.c_str());
            return task_submitter_(
                RedisTask(
                    RedisOperationType::UPDATE_COMPANY_SUMMARY,
                    stock_id,
                    nullptr));
        }

        void setTaskSubmitter(TaskSubmitter submitter)
        {
            task_submitter_ = std::move(submitter);
        }

    private:
        std::unique_ptr<RedisPlusPlusClient<SummaryData, ErrorResult>> redisClient_; // Redis 客戶端
        std::unordered_map<std::string, SummaryData> summaryCacheData_;              // 本地緩存
        mutable std::shared_mutex cacheMutex_;                                       // <--- 新增: 用於保護 summaryCacheData_ 的讀寫鎖, mutable 允許在 const 方法中鎖定 (如果有的話)
        bool initRedisSearchIndex_ = false;
        TaskSubmitter task_submitter_;

        /**
         * @brief 將 SummaryData 序列化為 JSON 字串。 (此方法不存取 summaryCacheData_，是純函數)
         */
        Result<std::string, ErrorResult> summaryDataToJson(const SummaryData *data) const // 標記為 const
        {
            try
            {
                if (data == nullptr)
                {
                    LOG_F(ERROR, "RedisSummaryAdapter:summaryDataToJson Unexpact Summary Data null");
                    return Result<std::string, ErrorResult>::Err(
                        ErrorResult{ErrorCode::UnexpectedError, "RedisSummaryAdapter:summaryDataToJson summary_data = nullptr"});
                }

                nlohmann::json j;
                j["stock_id"] = data->stock_id;
                j["area_center"] = data->area_center;
                j["margin_available_amount"] = data->margin_available_amount;
                j["margin_available_qty"] = data->margin_available_qty;
                j["short_available_amount"] = data->short_available_amount;
                j["short_available_qty"] = data->short_available_qty;
                j["after_margin_available_amount"] = data->after_margin_available_amount;
                j["after_margin_available_qty"] = data->after_margin_available_qty;
                j["after_short_available_amount"] = data->after_short_available_amount;
                j["after_short_available_qty"] = data->after_short_available_qty;
                j["belong_branches"] = data->belong_branches;
                return Result<std::string, ErrorResult>::Ok(j.dump());
            }
            catch (const std::exception &ex)
            {
                return Result<std::string, ErrorResult>::Err(
                    ErrorResult{ErrorCode::JsonParseError, ex.what()});
            }
        }

        /**
         * @brief 將 JSON 反序列化為 SummaryData。 (此方法不存取 summaryCacheData_，是純函數)
         */
        Result<SummaryData, ErrorResult> jsonToSummaryData(const std::string &jsonStr) const // 標記為 const
        {
            try
            {
                nlohmann::json j = nlohmann::json::parse(jsonStr);
                if (j.is_array() && !j.empty()) // RedisJSON 的 JSON.GET $ 返回的是陣列
                    j = j[0];

                SummaryData data; // 直接使用 SummaryData，不再是指標
                data.stock_id = j.at("stock_id").get<std::string>();
                data.area_center = j.at("area_center").get<std::string>();
                data.margin_available_amount = j.at("margin_available_amount").get<int64_t>();
                data.margin_available_qty = j.at("margin_available_qty").get<int64_t>();
                data.short_available_amount = j.at("short_available_amount").get<int64_t>();
                data.short_available_qty = j.at("short_available_qty").get<int64_t>();
                data.after_margin_available_amount = j.at("after_margin_available_amount").get<int64_t>();
                data.after_margin_available_qty = j.at("after_margin_available_qty").get<int64_t>();
                data.after_short_available_amount = j.at("after_short_available_amount").get<int64_t>();
                data.after_short_available_qty = j.at("after_short_available_qty").get<int64_t>();
                data.belong_branches = j.at("belong_branches").get<std::vector<std::string>>();
                return Result<SummaryData, ErrorResult>::Ok(std::move(data));
            }
            catch (const std::exception &ex)
            {
                return Result<SummaryData, ErrorResult>::Err(
                    ErrorResult{ErrorCode::JsonParseError,
                                "解析JSON失敗: " + std::string(ex.what())});
            }
        }

        /**
         * @brief 加載並快取多個 key 的數據 (此方法假設已被外部獨佔鎖保護)
         * @param keys 要加載的 key 列表
         * @return Result<void> 操作結果
         */
        Result<void, ErrorResult> loadAndCacheKeysData(const std::vector<std::string> &keys)
        {
            std::unique_lock<std::shared_mutex> lock(cacheMutex_);
            summaryCacheData_.clear(); // 寫操作
            size_t loaded = 0;
            for (const auto &key : keys)
            {
                auto jsonRes = redisClient_->getJson(key, "$"); // 讀取 Redis
                if (jsonRes.is_err())
                {
                    LOG_F(WARNING, "JSON.GET '%s' 失敗: %s",
                          key.c_str(), jsonRes.unwrap_err().message.c_str());
                    continue;
                }

                auto parseRes = jsonToSummaryData(jsonRes.unwrap()); // 解析 JSON
                if (parseRes.is_err())
                {
                    LOG_F(WARNING, "解析 '%s' JSON 失敗: %s",
                          key.c_str(), parseRes.unwrap_err().message.c_str());
                    continue;
                }
                // 寫入快取
                summaryCacheData_[key] = parseRes.unwrap(); // 寫操作
                loaded++;
            }
            LOG_F(INFO, "已從 Redis 載入 %zu 筆 summary 資料。", loaded);
            LOG_F(INFO, "Summary Cache Data 資料 : %zu 筆 。", summaryCacheData_.size());
            return Result<void, ErrorResult>::Ok();
        }
    };
} // namespace finance::infrastructure::storage
