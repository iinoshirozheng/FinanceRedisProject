#include "LoguruLogger.h"
#include <cstdarg>

namespace finance::infrastructure::logging
{
    void LoguruLogger::log(LogLevel level, const char *file, int line, const char *func, const char *format, ...)
    {
        va_list args;
        va_start(args, format);

        switch (level)
        {
        case LogLevel::TRACE:
            loguru::log(2, file, line, func, format, args);
            break;
        case LogLevel::DEBUG:
            loguru::log(1, file, line, func, format, args);
            break;
        case LogLevel::INFO:
            loguru::log(0, file, line, func, format, args);
            break;
        case LogLevel::WARNING:
            loguru::log(loguru::Verbosity_WARNING, file, line, func, format, args);
            break;
        case LogLevel::ERROR:
            loguru::log(loguru::Verbosity_ERROR, file, line, func, format, args);
            break;
        case LogLevel::FATAL:
            loguru::log(loguru::Verbosity_FATAL, file, line, func, format, args);
            break;
        }

        va_end(args);
    }

    void LoguruLogger::setLogLevel(LogLevel level)
    {
        loguru::g_stderr_verbosity = static_cast<int>(level);
    }

    void LoguruLogger::addFileOutput(const std::string &path)
    {
        loguru::add_file(path.c_str(),
                         loguru::Append,
                         loguru::Verbosity_MAX);
    }

    void LoguruLogger::configureAsync(bool enable_async, size_t flush_interval_secs)
    {
        async_enabled_ = enable_async;
        flush_interval_secs_ = flush_interval_secs;
    }

    void LoguruLogger::cleanup()
    {
        // 确保所有日志都被写入
        loguru::flush();
    }

} // namespace finance::infrastructure::logging