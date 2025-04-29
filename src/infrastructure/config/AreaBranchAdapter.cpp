#include "AreaBranchAdapter.h"
#include "../../../lib/loguru/loguru.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>

namespace finance
{
    namespace infrastructure
    {
        namespace storage
        {

            using json = nlohmann::json;

            bool AreaBranchAdapter::loadFromFile(const std::string &filename)
            {
                try
                {
                    std::ifstream file(filename);
                    if (!file.is_open())
                    {
                        return false;
                    }

                    nlohmann::json jsonData;
                    file >> jsonData;

                    // Clear existing data
                    validAreaBranches.clear();

                    // Load valid area branches
                    if (jsonData.contains("valid_area_branch") && jsonData["valid_area_branch"].is_array())
                    {
                        validAreaBranches = jsonData["valid_area_branch"].get<std::vector<std::string>>();
                    }

                    return true;
                }
                catch (const std::exception &)
                {
                    return false;
                }
            }

            std::vector<std::string> AreaBranchAdapter::getValidAreaBranches() const
            {
                return validAreaBranches;
            }

            bool AreaBranchAdapter::isValidAreaBranch(const std::string &areaBranch) const
            {
                return std::find(validAreaBranches.begin(), validAreaBranches.end(), areaBranch) != validAreaBranches.end();
            }

            void AreaBranchAdapter::initializeMaps()
            {
                // 清除現有數據
                areaIds_.clear();
                validAreaBranches_.clear();

                // 從 JSON 數據初始化
                for (auto &[key, value] : areaData_.items())
                {
                    areaIds_.insert(key);

                    try
                    {
                        std::vector<std::string> branches = value.get<std::vector<std::string>>();
                        for (const auto &branch : branches)
                        {
                            validAreaBranches_.push_back(branch);
                        }
                    }
                    catch (const std::exception &ex)
                    {
                        LOG_F(ERROR, "Invalid branch data for area %s: %s", key.c_str(), ex.what());
                    }
                }

                LOG_F(INFO, "Loaded %zu areas and %zu branches", areaIds_.size(), validAreaBranches_.size());
            }

            std::vector<std::string> AreaBranchAdapter::getBranchesForArea(const std::string &areaCenter)
            {
                if (areaIds_.find(areaCenter) == areaIds_.end())
                {
                    // 未找到區域
                    return {};
                }

                try
                {
                    return areaData_[areaCenter].get<std::vector<std::string>>();
                }
                catch (const std::exception &ex)
                {
                    LOG_F(ERROR, "Failed to get branches for area %s: %s", areaCenter.c_str(), ex.what());
                    return {};
                }
            }

            std::vector<std::string> AreaBranchAdapter::getAllAreas()
            {
                return std::vector<std::string>(areaIds_.begin(), areaIds_.end());
            }

            std::vector<std::string> AreaBranchAdapter::getValidAreaBranches() const
            {
                return validAreaBranches_;
            }

        } // namespace storage
    } // namespace infrastructure
} // namespace finance