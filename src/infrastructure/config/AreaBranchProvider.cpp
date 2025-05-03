#include "AreaBranchProvider.hpp"
#include <loguru.hpp>

namespace finance::infrastructure::config
{
    AreaBranchProvider::AreaBranchProvider()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (jsonData_.empty())
        {
            // Initialize with default data
            jsonData_ = nlohmann::json::object();
        }
    }

    bool AreaBranchProvider::loadFromFile(const std::string &filePath)
    {
        if (!JsonProviderBase::loadFromFile(filePath))
        {
            LOG_F(ERROR, "Failed to load file: %s", filePath.c_str());
            return false;
        }

        std::lock_guard<std::mutex> lock(mutex_);
        try
        {
            for (auto &[key, value] : JsonProviderBase::getJsonData().items())
            {
                backoffice_ids_.push_back(key);
                for (auto &branch : value)
                {
                    auto branch_id = branch.get<std::string>();
                    followingBrokerIds_.push_back(branch_id);
                    allBranchs_.push_back(branch_id);
                }
            }
            return true;
        }
        catch (const std::exception &e)
        {
            LOG_F(ERROR, "Failed to parse JSON data: %s", e.what());
            return false;
        }
    }

    std::vector<std::string> AreaBranchProvider::getBranchesForArea(const std::string &areaId) const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (jsonData_.empty())
        {
            LOG_F(ERROR, "JSON data is empty");
            return {};
        }

        try
        {
            if (auto it = jsonData_.find(areaId); it != jsonData_.end())
            {
                return it->get<std::vector<std::string>>();
            }
            return {};
        }
        catch (const nlohmann::json::exception &e)
        {
            LOG_F(ERROR, "Failed to get branches for area %s: %s", areaId.c_str(), e.what());
            return {};
        }
    }

    std::vector<std::string> AreaBranchProvider::getAllBranches() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return allBranchs_;
    }

    std::vector<std::string> AreaBranchProvider::getBackofficeIds() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return backoffice_ids_;
    }

    std::vector<std::string> AreaBranchProvider::getFollowingBrokerIds() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return followingBrokerIds_;
    }
} // namespace finance::infrastructure::config