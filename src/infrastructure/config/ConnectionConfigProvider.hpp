#pragma once

#include "./JsonProviderBase.hpp"
#include "loguru.hpp"
#include <string>

namespace finance::infrastructure::config
{

    /**
     * ConnectionConfigProvider 的實現，從 JSON 文件加載配置
     */
    class ConnectionConfigProvider : public JsonProviderBase
    {
    public:
        ConnectionConfigProvider() = default;
        explicit ConnectionConfigProvider(const std::string &filePath)
        {
            if (!loadFromFile(filePath))
            {
                LOG_F(ERROR, "Failed to load configuration from file: %s", filePath.c_str());
            }
        }
        ~ConnectionConfigProvider() override = default;

        /**
         * 獲取配置數據
         * @return 配置數據
         */
        std::string getRedisUrl() const
        {
            if (isJsonDataEmpty())
            {
                LOG_F(ERROR, "JSON data is empty");
                return "";
            }

            try
            {
                return jsonData_["redis_url"].get<std::string>();
            }
            catch (const nlohmann::json::exception &e)
            {
                LOG_F(ERROR, "Failed to get redis_url: %s", e.what());
                return "";
            }
        }

        int getServerPort() const
        {
            if (isJsonDataEmpty())
            {
                LOG_F(ERROR, "JSON data is empty");
                return -1;
            }

            try
            {
                return jsonData_["server_port"].get<int>();
            }
            catch (const nlohmann::json::exception &e)
            {
                LOG_F(ERROR, "Failed to get server_port: %s", e.what());
                return -1;
            }
        }

        bool IsInitializeIndices() const { return initializeIndices_; }
        void Reset()
        {
            redisUrl_ = "";
            serverPort_ = 0;
            initializeIndices_ = false;
        }

        /**
         * 從文件加載配置
         * @param filePath 配置文件路徑
         * @return 如果成功加載則返回真
         */
        bool loadFromFile(const std::string &filePath) override
        {
            if (!JsonProviderBase::loadFromFile(filePath))
            {
                return false; // 父類加載失敗則返回 false
            }

            nlohmann::json jsonData = JsonProviderBase::getJsonData();
            try
            {
                // 提取特定配置項目
                redisUrl_ = jsonData.at("redisUrl").get<std::string>();
                serverPort_ = jsonData.at("serverPort").get<int64_t>();
                initializeIndices_ = jsonData.at("initializeIndices").get<bool>();
            }
            catch (const std::exception &ex)
            {
                // LOG_F(ERROR, "Error extracting config: " + std::string(ex.what()));
                return false; // 如果解析出現錯誤，返回 false
            }

            return true; // 加載成功返回 true
        }

    private:
        std::string redisUrl_;
        int64_t serverPort_;
        bool initializeIndices_;
    };

} // namespace finance::infrastructure::config