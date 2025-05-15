#pragma once

#include "domain/Result.hpp"
#include "domain/FinanceDataStructure.hpp"
#include "utils/FinanceUtils.hpp"
#include "infrastructure/config/ConnectionConfigProvider.hpp"
#include "infrastructure/config/AreaBranchProvider.hpp"
#include "RedisPlusPlusClient.hpp"
#include "domain/IFinanceRepository.hpp"
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
        Result<void, ErrorResult> init() override
        {
            if (redisClient_)
                return Result<void, ErrorResult>::Ok();

            auto client = std::make_unique<RedisPlusPlusClient<SummaryData, ErrorResult>>();
            std::string uri = config::ConnectionConfigProvider::redisUri();
            std::string password = config::ConnectionConfigProvider::redisPassword();
            return client->connect(uri, password)
                .and_then([&] { // ← 這裡不帶參數
                    this->redisClient_ = std::move(client);
                    return Result<void, ErrorResult>::Ok();
                })
                .map_err([](const ErrorResult &e)
                         { return ErrorResult{e.code, "Redis 連線失敗: " + e.message}; });
        }

        /**
         * @brief 序列化並同步資料到 Redis，同時更新本地緩存。
         * @param key 要同步的 key
         * @param data 要同步的 SummaryData 資料
         * @return Result<void> 操作結果
         */
        Result<void, ErrorResult> sync(const std::string &key, const SummaryData *data) override
        {
            if (!redisClient_)
            {
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::RedisConnectionFailed, "Redis 未正確連線"});
            }

            if (data == nullptr)
            {
                LOG_F(ERROR, "RedisSummaryAdapter:Unexpact Summary Data null, key=%s", key.c_str());
                return Result<void, ErrorResult>::Err(
                    ErrorResult{ErrorCode::UnexpectedError, "RedisSummaryAdapter:summary_data = nullptr"});
            }

            return summaryDataToJson(data)
                .and_then([this, &key](const std::string &j)
                          { return redisClient_->setJson(key, "$", j); })
                // .and_then([this, &key, &d]
                //           { return setData(key, d); })
                .map_err([&](const ErrorResult &e)
                         { return ErrorResult{e.code, "Sync 失敗: " + e.message}; });
        }

        /**
         * @brief 從本地緩存或 Redis 中讀取資料。
         * @param key 完整的 Redis Key，例如 "summary:AREA:STOCK"
         * @return Result<SummaryData> 查詢結果
         */
        Result<domain::SummaryData *, ErrorResult> getData(const std::string &key) override
        {
            // 嘗試查找 key 是否存在於 summaryCacheData_ 中
            if (auto it = summaryCacheData_.find(key); it != summaryCacheData_.end())
            {
                // 如果找到該 key，返回成功結果，並返回 SummaryData 的引用
                return Result<domain::SummaryData *, ErrorResult>::Ok(&(it->second));
            }
            else
            {
                domain::SummaryData data;
                // 如果未找到，創建新的 SummaryData 並插入到 summaryCacheData_
                auto [newIt, inserted] = summaryCacheData_.emplace(key, data);
                if (inserted)
                {
                    // 插入成功，返回新建 SummaryData 的引用
                    return Result<domain::SummaryData *, ErrorResult>::Ok(&(newIt->second));
                }
                else
                {
                    // 理論上不會進到這裡，但仍然提供錯誤結果以防異常情況
                    return Result<domain::SummaryData *, ErrorResult>::Err(
                        ErrorResult{ErrorCode::UnexpectedError, "無法插入新的 SummaryData"});
                }
            }
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
            return redisClient_->keys(pattern)
                .and_then([this](const std::vector<std::string> &keys)
                          { return loadAndCacheKeysData(keys); })
                .map_err([](const ErrorResult &e)
                         { return ErrorResult{e.code, "LoadAll 操作失敗: " + e.message}; });
        }

        /**
         * @brief 實現 IFinanceRepository 接口的 set 方法
         * @param key 要設置的 key
         * @return Result<void> 操作結果
         */
        Result<void, ErrorResult> setData(const std::string &key, const SummaryData &data) override
        {
            summaryCacheData_[key] = std::move(data);
            return Result<void, ErrorResult>::Ok();
        }

        /**
         * @brief 實現 IFinanceRepository 接口的 update 方法
         * @param key 要更新的 key
         * @return Result<void> 操作結果
         */
        Result<void, ErrorResult> update(const std::string &stock_id) override
        {
            if (!redisClient_)
            {
                return Result<void, ErrorResult>::Err(ErrorResult{ErrorCode::RedisConnectionFailed, "Redis 未正確連線"});
            }

            // Summary data 加總
            struct SummaryData company_summary;
            company_summary.stock_id = stock_id;
            company_summary.area_center = "ALL";
            company_summary.belong_branches = config::AreaBranchProvider::getAllBranches();

            for (const std::string &officeId : config::AreaBranchProvider::getBackofficeIds())
            {
                const std::string key = "summary:" + officeId + ":" + stock_id;
                if (auto it = summaryCacheData_.find(key); it != summaryCacheData_.end())
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
            auto res = redisClient_->del(key);
            if (res.is_ok())
            {
                summaryCacheData_.erase(key);
                return true;
            }
            return false;
        }

    private:
        std::unique_ptr<RedisPlusPlusClient<SummaryData, ErrorResult>> redisClient_; // Redis 客戶端
        std::unordered_map<std::string, SummaryData> summaryCacheData_;              // 本地緩存

        /**
         * @brief 將 SummaryData 序列化為 JSON 字串。
         */
        Result<std::string, ErrorResult> summaryDataToJson(const SummaryData *data)
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
         * @brief 將 JSON 反序列化為 SummaryData。
         */
        Result<SummaryData, ErrorResult> jsonToSummaryData(const std::string &jsonStr)
        {
            try
            {
                nlohmann::json j = nlohmann::json::parse(jsonStr);
                if (j.is_array() && !j.empty())
                    j = j[0];

                struct SummaryData data;
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
         * @brief 加載並快取多個 key 的數據
         * @param keys 要加載的 key 列表
         * @return Result<void> 操作結果
         */
        Result<void, ErrorResult> loadAndCacheKeysData(const std::vector<std::string> &keys)
        {
            summaryCacheData_.clear();
            size_t loaded = 0;
            for (const auto &key : keys)
            {
                auto jsonRes = redisClient_->getJson(key, "$");
                if (jsonRes.is_err())
                {
                    LOG_F(WARNING, "JSON.GET '%s' 失敗: %s",
                          key.c_str(), jsonRes.unwrap_err().message.c_str());
                    continue;
                }

                auto parseRes = jsonToSummaryData(jsonRes.unwrap());
                if (parseRes.is_err())
                {
                    LOG_F(WARNING, "解析 '%s' JSON 失敗: %s",
                          key.c_str(), parseRes.unwrap_err().message.c_str());
                    continue;
                }

                summaryCacheData_[key] = parseRes.unwrap();
                loaded++;
            }
            LOG_F(INFO, "已從 Redis 載入 %zu 筆 summary 資料。", loaded);
            LOG_F(INFO, "Summary Cache Data 資料 : %zu 筆 。", summaryCacheData_.size());
            return Result<void, ErrorResult>::Ok();
        }
    };
} // namespace finance::infrastructure::storage