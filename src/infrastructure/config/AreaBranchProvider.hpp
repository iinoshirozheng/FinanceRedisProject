#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <nlohmann/json.hpp>
#include <loguru.hpp>
#include <mutex>
#include <algorithm>
#include <vector> // 包含 vector

namespace finance::infrastructure::config
{

    class AreaBranchProvider
    {
    public:
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

                    backofficeIdsSet_.clear();
                    allBranchesSet_.clear(); // 改為 unordered_set
                    followingBrokerIdsSet_.clear(); // 改為 unordered_set
                    areaToBranches_.clear();

                    for (auto& [areaId, branches] : jsonData_.items())
                    {
                        backofficeIdsSet_.insert(areaId);
                        auto& vec = areaToBranches_[areaId];
                        for (auto& b : branches) {
                            std::string branchId = b.get<std::string>();
                            vec.push_back(branchId);
                            allBranchesSet_.insert(branchId); // 插入到 unordered_set
                            followingBrokerIdsSet_.insert(branchId); // 插入到 unordered_set
                        }
                    }
                    // *** 新增：將所有分支 ID 從 set 複製到 vector ***
                    allBranchesVec_.assign(allBranchesSet_.begin(), allBranchesSet_.end());
                    backofficeIdsVec_.assign(backofficeIdsSet_.begin(), backofficeIdsSet_.end()); });

                return true;
            }
            catch (const std::exception &e)
            {
                LOG_F(ERROR, "AreaBranchProvider load failed: %s", e.what());
                return false;
            }
        }

        inline static const std::vector<std::string> &getBranchesForArea(const std::string &areaId) noexcept
        {
            auto it = areaToBranches_.find(areaId);
            if (it != areaToBranches_.end())
            {
                return it->second;
            }
            static const std::vector<std::string> emptyVec;
            return emptyVec;
        }

        // 新增：返回所有分支 ID 的 vector
        inline static const std::vector<std::string> &getAllBranches() noexcept { return allBranchesVec_; }

        // 返回所有分支 ID (改為檢查是否存在)
        inline static bool IsBranchValid(const std::string &branchId) noexcept { return allBranchesSet_.count(branchId) > 0; }

        inline static const std::vector<std::string> &getBackofficeIds() noexcept { return backofficeIdsVec_; }
        // 返回所有分支對應列表 (改為檢查是否存在)
        inline static bool IsFollowingBrokerId(const std::string &brokerId) noexcept { return followingBrokerIdsSet_.count(brokerId) > 0; }

        inline static bool IsValidAreaCenter(const std::string &area) noexcept { return backofficeIdsSet_.count(area) > 0; }

    private:
        inline static std::once_flag initFlag_{};
        inline static nlohmann::json jsonData_{};
        inline static std::unordered_set<std::string> backofficeIdsSet_{};
        inline static std::vector<std::string> backofficeIdsVec_{};
        inline static std::unordered_set<std::string> allBranchesSet_{};        // 改為 unordered_set
        inline static std::vector<std::string> allBranchesVec_{};               // *** 新增：儲存所有分支 ID 的 vector ***
        inline static std::unordered_set<std::string> followingBrokerIdsSet_{}; // 改為 unordered_set
        inline static std::unordered_map<std::string, std::vector<std::string>> areaToBranches_{};
    };

} // namespace finance::infrastructure::config