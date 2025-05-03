#pragma once

#include "JsonProviderBase.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <nlohmann/json.hpp>

namespace finance::infrastructure::config
{
    /**
     * AreaBranchProvider 的實現，從 JSON 文件加載配置
     */
    class AreaBranchProvider : public JsonProviderBase
    {
    public:
        AreaBranchProvider();
        ~AreaBranchProvider() = default;

        bool loadFromFile(const std::string &filePath) override;
        std::vector<std::string> getBranchesForArea(const std::string &areaId) const;
        std::vector<std::string> getAllBranches() const;
        std::vector<std::string> getBackofficeIds() const;
        std::vector<std::string> getFollowingBrokerIds() const;

    private:
        mutable std::mutex mutex_;
        nlohmann::json jsonData_;
        std::vector<std::string> allBranchs_;
        std::vector<std::string> backoffice_ids_;
        std::vector<std::string> followingBrokerIds_;
    };
} // namespace finance::infrastructure::config
