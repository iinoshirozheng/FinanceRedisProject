#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <nlohmann/json.hpp>
#include <loguru.hpp>

namespace finance::infrastructure::config
{

    class AreaBranchProvider
    {
    public:
        // 單次從 JSON 檔載入設定並初始化靜態快取
        inline static bool loadFromFile(const std::string &filePath)
        {
            try
            {
                std::call_once(initFlag_, [&]()
                               {
                std::ifstream ifs(filePath);
                if (!ifs) throw std::runtime_error("Cannot open config file: " + filePath);

                jsonData_ = nlohmann::json::parse(ifs);
                if (!jsonData_.is_object()) throw std::runtime_error("Invalid JSON format: expected object");

                // 清理舊資料
                backofficeIds_.clear();
                allBranches_.clear();
                followingBrokerIds_.clear();
                areaToBranches_.clear();

                // 解析各區域與分支，並填充快取映射
                for (auto& [areaId, branches] : jsonData_.items()) {
                    backofficeIds_.push_back(areaId);  // 記錄區域 ID
                    auto& vec = areaToBranches_[areaId];
                    for (auto& b : branches) {
                        std::string branchId = b.get<std::string>();
                        vec.push_back(branchId);           // 快取分支列表
                        allBranches_.push_back(branchId);  // 全部分支列表
                        followingBrokerIds_.push_back(branchId);
                    }
                } });
                return true;
            }
            catch (const std::exception &e)
            {
                LOG_F(ERROR, "AreaBranchProvider load failed: %s", e.what());
                return false;
            }
        }

        // 返回某區域下的所有分支，若不存在返回空集合
        inline static const std::vector<std::string> &getBranchesForArea(const std::string &areaId)
        {
            auto it = areaToBranches_.find(areaId);
            if (it != areaToBranches_.end())
            {
                return it->second;
            }
            static const std::vector<std::string> emptyVec;
            return emptyVec;
        }

        // 返回所有分支 ID
        inline static const std::vector<std::string> &getAllBranches()
        {
            return allBranches_;
        }

        // 返回所有區域 ID
        inline static const std::vector<std::string> &getBackofficeIds()
        {
            return backofficeIds_;
        }

        // 返回所有分支對應列表
        inline static const std::vector<std::string> &getFollowingBrokerIds()
        {
            return followingBrokerIds_;
        }

    private:
        inline static std::once_flag initFlag_{};
        inline static nlohmann::json jsonData_{};
        inline static std::vector<std::string> backofficeIds_{};
        inline static std::vector<std::string> allBranches_{};
        inline static std::vector<std::string> followingBrokerIds_{};
        inline static std::unordered_map<std::string, std::vector<std::string>> areaToBranches_{};
    };

} // namespace finance::infrastructure::config
