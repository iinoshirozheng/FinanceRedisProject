#pragma once
#include <nlohmann/json.hpp>
#include "loguru.hpp"
#include <fstream>
namespace finance
{
    namespace infrastructure
    {
        namespace config
        {
            // 提供 Config 配置數據介面
            class JsonProviderBase
            {
            public:
                JsonProviderBase() = default;
                virtual ~JsonProviderBase() = default;

                inline virtual void setJsonData(const nlohmann::json &newData) { jsonData_ = newData; }

                // 獲取配置數據
                // @return 配置數據
                inline const nlohmann::json &getJsonData() const { return jsonData_; }

                // 從文件中載入配置
                // @param filePath 配置文件路徑
                // @return 如果載入成功則返回 true
                inline virtual bool loadFromFile(const std::string &filePath)
                {
                    this->clearJsonData();

                    std::ifstream input_stream(filePath);
                    if (!input_stream.is_open())
                    {
                        LOG_F(ERROR, "Failed to open file: %s", filePath.c_str());
                        return false;
                    }

                    try
                    {
                        input_stream >> jsonData_;
                    }
                    catch (const nlohmann::json::parse_error &e)
                    {
                        LOG_F(ERROR, "Failed to parse JSON from file %s: %s", filePath.c_str(), e.what());
                        jsonData_.clear(); // 清空此前數據，確保數據一致
                        return false;
                    }

                    LOG_F(INFO, "Successfully loaded JSON from file: %s", filePath.c_str());
                    return true;
                }

                // 確認是不是沒有 load 資料
                inline bool isJsonDataEmpty() const { return jsonData_.empty(); }

                // 清除資料
                inline void clearJsonData() { jsonData_.clear(); }

            protected:
                nlohmann::json jsonData_;
            };

        } // namespace config

    } // namespace infrastructure

} // namespace finance
