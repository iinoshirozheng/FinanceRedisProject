#pragma once

#include "./FinanceDataStructure.h"
#include "./Result.hpp"

namespace finance::domain
{
    // 通用數據處理器介面，基於模板的方式
    class IPackageHandler
    {
    public:
        virtual ~IPackageHandler() = default;

        // 處理數據
        // @param data 要處理的數據（模板類型 T）
        // @param optionalHeader 可選的頭部信息
        // @return 如果處理成功則返回 true
        virtual Result<SummaryData> processData(const struct domain::ApData &data) = 0;
    };

} // namespace finance::domain