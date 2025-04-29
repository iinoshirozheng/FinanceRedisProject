#pragma once

#include "../../domain/IFinanceRepository.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <memory>
#include "nlohmann/json.hpp"

namespace finance
{
    namespace infrastructure
    {
        namespace storage
        {
            /**
             * @brief Adapter for handling area-branch mappings
             */
            class AreaBranchAdapter
            {
            public:
                AreaBranchAdapter() = default;
                ~AreaBranchAdapter() = default;

                /**
                 * @brief Load area-branch mappings from a JSON file
                 * @param filename The JSON file to load from
                 * @return true if successful, false otherwise
                 */
                bool loadFromFile(const std::string &filename);

                /**
                 * @brief Get all valid area branches
                 * @return Vector of valid area branch codes
                 */
                std::vector<std::string> getValidAreaBranches() const;

                /**
                 * @brief Check if an area branch is valid
                 * @param areaBranch The area branch code to check
                 * @return true if valid, false otherwise
                 */
                bool isValidAreaBranch(const std::string &areaBranch) const;

                /**
                 * @brief Initialize internal maps from JSON data
                 */
                void initializeMaps();

                /**
                 * @brief Get all branches for a specific area
                 * @param areaCenter The area center code
                 * @return Vector of branch codes
                 */
                std::vector<std::string> getBranchesForArea(const std::string &areaCenter);

                /**
                 * @brief Get the area for a specific branch
                 * @param branchId The branch code
                 * @return The area center code
                 */
                std::string getAreaForBranch(const std::string &branchId);

                /**
                 * @brief Get all area centers
                 * @return Vector of area center codes
                 */
                std::vector<std::string> getAllAreas();

                /**
                 * @brief Get all branches
                 * @return Vector of branch codes
                 */
                std::vector<std::string> getValidAreaBranches() const;

            private:
                std::vector<std::string> validAreaBranches;
                std::set<std::string> areaIds_;
                std::vector<std::string> validAreaBranches_;
                nlohmann::json areaData_;
            };

        } // namespace storage
    } // namespace infrastructure
} // namespace finance