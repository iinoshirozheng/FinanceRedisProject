#pragma once

#include "../../domain/IFinanceRepository.h"
#include <string>

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
                ConfigAdapter();

                /**
                 * 獲取配置數據
                 * @return 配置數據
                 */
                domain::ConfigData getConfig() override;

                /**
                 * 從文件加載配置
                 * @param filePath 配置文件路徑
                 * @return 如果成功加載則返回真
                 */
                bool loadFromFile(const std::string &filePath) override;

            private:
                domain::ConfigData config_;
            };

        } // namespace storage
    } // namespace infrastructure
} // namespace finance