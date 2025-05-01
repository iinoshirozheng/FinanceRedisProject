#pragma once

#include "ILogger.h"
#include <loguru.hpp>

namespace finance::infrastructure::logging
{
    class LoguruLogger : public ILogger
    {
    public:
        LoguruLogger() = default;
        ~LoguruLogger() override = default;

        void log(LogLevel level, const char *file, int line, const char *func, const char *format, ...) override;
        void setLogLevel(LogLevel level) override;
        void addFileOutput(const std::string &path) override;

        // 新增：配置异步写入
        void configureAsync(bool enable_async = true, size_t flush_interval_secs = 0);
        // 新增：清理资源
        void cleanup();

    private:
        bool async_enabled_ = false;
        size_t flush_interval_secs_ = 0;
    };

} // namespace finance::infrastructure::logging