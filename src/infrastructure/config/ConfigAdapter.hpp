#pragma once

#include "../../domain/FinanceDataStructures.h"
#include "../../domain/IFinanceRepository.h"
#include <nlohmann/json.hpp>
#include <string>
#include <fstream>
#include "../../../lib/loguru/loguru.hpp"

namespace finance
{
    namespace infrastructure
    {
        namespace storage
        {

            /**
             * IConfigProvider 的實現，從 JSON 文件加載配置
             */
            class ConfigAdapter : public domain::IConfigProvider
            {
            public:
                /**
                 * 構造函數
                 */
                ConfigAdapter()
                {
                    config_.redisUrl = "";
                    config_.serverPort = 0;
                    config_.initializeIndices = false;
                }

                /**
                 * 獲取配置數據
                 * @return 配置數據
                 */
                inline domain::ConfigData getConfig() override
                {
                    return config_;
                }

                inline bool empty() override
                {
                    return config_.redisUrl == "" && config_.serverPort == 0 && config_.initializeIndices == false;
                }

                /**
                 * 從文件加載配置
                 * @param filePath 配置文件路徑
                 * @return 如果成功加載則返回真
                 */
                inline bool loadFromFile(const std::string &filePath) override
                {
                    std::ifstream file(filePath);
                    if (!file)
                    {
                        LOG_F(ERROR, "Failed to open configuration file: %s", filePath.c_str());
                        return false;
                    }

                    try
                    {
                        nlohmann::json j;
                        file >> j;

                        config_.redisUrl = j.value("redis_url", config_.redisUrl);
                        config_.serverPort = j.value("server_port", config_.serverPort);

                        LOG_F(INFO, "Loaded configuration: redis_url=%s, server_port=%d, initialize_indices=%d",
                              config_.redisUrl.c_str(), config_.serverPort, config_.initializeIndices);

                        return true;
                    }
                    catch (const std::exception &ex)
                    {
                        LOG_F(ERROR, "Failed to parse configuration file: %s", ex.what());
                        return false;
                    }
                }

            private:
                domain::ConfigData config_;
            };

        } // namespace storage
    } // namespace infrastructure
} // namespace finance