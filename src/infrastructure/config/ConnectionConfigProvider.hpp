#pragma once

#include <string>
#include <fstream>
#include <nlohmann/json.hpp>
#include <loguru.hpp>
#include <mutex>

namespace finance::infrastructure::config
{

    class ConnectionConfigProvider
    {
    public:
        // 單次從 JSON 檔載入設定並初始化靜態成員
        inline static bool loadFromFile(const std::string &filePath)
        {
            try
            {
                std::call_once(initFlag_, [&]()
                               {
                                   // 打開檔案，失敗即拋例外
                                   std::ifstream ifs(filePath);
                                   if (!ifs)
                                   {
                                       throw std::runtime_error("Cannot open config file: " + filePath);
                                   }
                                   // 解析完整 JSON
                                   jsonData_ = nlohmann::json::parse(ifs);
                                   // 格式檢查：必須是 JSON 物件
                                   if (!jsonData_.is_object())
                                   {
                                       throw std::runtime_error("Invalid config format: not a JSON object");
                                   }
                                   // 必填欄位檢查
                                   if (!jsonData_.contains("redis_url") ||
                                       !jsonData_.contains("server_port") ||
                                       !jsonData_.contains("socket_timeout_ms") ||
                                       !jsonData_.contains("redis_password"))
                                   {
                                       throw std::runtime_error("Missing required config fields");
                                   }
                                   // 依欄位類型取值，若型別錯誤則拋例外
                                   redisUrl_ = jsonData_.at("redis_url").get<std::string>();           // Redis 連線 URL
                                   redisPassword_ = jsonData_.at("redis_password").get<std::string>(); // Redis 密碼，可選
                                   serverPort_ = jsonData_.at("server_port").get<int>();               // 服務埠號
                                   socketTimeoutMs_ = jsonData_.at("socket_timeout_ms").get<int>();    // Socket 超時 (ms)
                               });
                return true; // 初次或重覆呼叫後皆回傳成功
            }
            catch (const std::exception &e)
            {
                // 載入或解析失敗時記錄錯誤
                LOG_F(ERROR, "ConnectionConfigProvider load failed: %s", e.what());
                return false;
            }
        }

        // 純讀：取得 Redis URL，確保已透過 loadFromFile() 初始化
        inline static const std::string &redisUri() noexcept
        {
            return redisUrl_; // e.g. "127.0.0.1:6379"
        }

        // 純讀：取得 Redis 密碼
        inline static const std::string &redisPassword() noexcept
        {
            return redisPassword_; // e.g. "your_password"
        }

        // 純讀：取得服務埠號
        inline static int serverPort() noexcept
        {
            return serverPort_; // e.g. 9516
        }

        // 純讀：取得 socket 超時時間 (ms)
        inline static int socketTimeoutMs() noexcept
        {
            return socketTimeoutMs_; // e.g. 5000
        }

    private:
        // 確保 JSON 只解析一次
        inline static std::once_flag initFlag_{};
        // 緩存解析後的設定資料
        inline static nlohmann::json jsonData_{};
        // 配置參數
        inline static std::string redisUrl_ = {};
        inline static std::string redisPassword_ = {}; // Redis 密碼
        inline static int serverPort_ = 0;
        inline static int socketTimeoutMs_ = 0;
    };

} // namespace finance::infrastructure::config
