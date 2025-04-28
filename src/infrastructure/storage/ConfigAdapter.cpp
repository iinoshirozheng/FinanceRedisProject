#include "ConfigAdapter.h"
#include "../../../lib/loguru/loguru.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>

namespace finance
{
    namespace infrastructure
    {
        namespace storage
        {
            using json = nlohmann::json;

            ConfigAdapter::ConfigAdapter()
            {
                // 初始化為默認值
                config_.redisUrl = "tcp://127.0.0.1:6479";
                config_.serverPort = 9516;
                config_.initializeIndices = false;
            }

            domain::ConfigData ConfigAdapter::getConfig()
            {
                return config_;
            }

            bool ConfigAdapter::loadFromFile(const std::string &filePath)
            {
                std::ifstream file(filePath);
                if (!file)
                {
                    LOG_F(ERROR, "Failed to open configuration file: %s", filePath.c_str());
                    return false;
                }

                try
                {
                    json j;
                    file >> j;

                    config_.redisUrl = j["redis_url"].get<std::string>();
                    config_.serverPort = j["server_port"].get<int>();
                    config_.initializeIndices = j["initialize_indices"].get<bool>();

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

        } // namespace storage
    } // namespace infrastructure
} // namespace finance