#pragma once
#include <map>
#include <string>
#include <vector>
#include <set>
#include "./JsonProviderBase.hpp"

namespace finance::infrastructure::config
{
    /**
     * AreaBranchProvider 的實現，從 JSON 文件加載配置
     */
    class AreaBranchProvider : public JsonProviderBase
    {
    public:
        AreaBranchProvider() = default;
        explicit AreaBranchProvider(const std::string &redisUrl) : redisUrl_(redisUrl) {}
        ~AreaBranchProvider() override = default;

        inline virtual bool loadFromFile(const std::string &filePath) override
        {
            if (!JsonProviderBase::loadFromFile(filePath))
            {
                // TODO: LOG()
                return false;
            }

            for (auto &[key, value] : JsonProviderBase::getJsonData().items())
            {
                // std::cout << "key:" << key << "value:" << value << std::endl;
                backoffice_ids_.insert(key);
                for (auto &branch : value)
                {
                    auto branch_id = branch.get<std::string>();
                    followingBrokerIds_.emplace(branch_id, key);
                    allBranchs_.emplace_back(branch_id);
                }
            }
            return true;
        }

        std::string getAreaCenterByBranchId(const std::string &branchId) const
        {
            if (isJsonDataEmpty())
            {
                LOG_F(ERROR, "JSON data is empty");
                return "";
            }

            try
            {
                return jsonData_["area_centers"][branchId].get<std::string>();
            }
            catch (const nlohmann::json::exception &e)
            {
                LOG_F(ERROR, "Failed to get area center for branch %s: %s", branchId.c_str(), e.what());
                return "";
            }
        }

        std::vector<std::string> getAllBranches() const
        {
            if (isJsonDataEmpty())
            {
                LOG_F(ERROR, "JSON data is empty");
                return {};
            }

            try
            {
                std::vector<std::string> branches;
                for (const auto &[branchId, _] : jsonData_["area_centers"].items())
                {
                    branches.push_back(branchId);
                }
                return branches;
            }
            catch (const nlohmann::json::exception &e)
            {
                LOG_F(ERROR, "Failed to get all branches: %s", e.what());
                return {};
            }
        }

        std::set<std::string> getAllBackofficeIds() const
        {
            if (isJsonDataEmpty())
            {
                LOG_F(ERROR, "JSON data is empty");
                return {};
            }

            try
            {
                return jsonData_["backoffice_ids"].get<std::set<std::string>>();
            }
            catch (const nlohmann::json::exception &e)
            {
                LOG_F(ERROR, "Failed to get backoffice IDs: %s", e.what());
                return {};
            }
        }

        std::vector<std::string> getBranchesForArea(const std::string &areaCenter) const
        {
            if (isJsonDataEmpty())
            {
                LOG_F(ERROR, "JSON data is empty");
                return {};
            }

            try
            {
                if (auto it = jsonData_.find(areaCenter); it != jsonData_.end())
                {
                    return it->get<std::vector<std::string>>();
                }
                return {};
            }
            catch (const nlohmann::json::exception &e)
            {
                LOG_F(ERROR, "Failed to get branches for area %s: %s", areaCenter.c_str(), e.what());
                return {};
            }
        }

    private:
        std::map<std::string, std::string> followingBrokerIds_;
        std::vector<std::string> allBranchs_;
        std::set<std::string> backoffice_ids_;
        std::string redisUrl_;
    };

} // namespace finance::infrastructure::config
