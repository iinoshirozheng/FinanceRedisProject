#pragma once

#include "FinanceDataStructure.hpp"
#include "Result.hpp"

namespace finance::domain
{
    // 通用數據處理器介面，基於模板的方式
    class IPackageHandler
    {
    public:
        virtual ~IPackageHandler() = default;
        // Only responsible for extracting ApData from the package and producing summary
        virtual Result<void, ErrorResult> handle(const FinancePackageMessage &pkg) = 0;
    };

} // namespace finance::domain