#pragma once

#include <string>
#include <memory>

namespace finance::infrastructure::logging
{
    enum class LogLevel
    {
        TRACE,
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    class ILogger
    {
    public:
        virtual ~ILogger() = default;

        virtual void log(LogLevel level, const char *file, int line, const char *func, const char *format, ...) = 0;
        virtual void setLogLevel(LogLevel level) = 0;
        virtual void addFileOutput(const std::string &path) = 0;
    };

// Convenience macros
#define LOG_TRACE(logger, ...) logger->log(LogLevel::TRACE, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_DEBUG(logger, ...) logger->log(LogLevel::DEBUG, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(logger, ...) logger->log(LogLevel::INFO, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARNING(logger, ...) logger->log(LogLevel::WARNING, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_ERROR(logger, ...) logger->log(LogLevel::ERROR, __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_FATAL(logger, ...) logger->log(LogLevel::FATAL, __FILE__, __LINE__, __func__, __VA_ARGS__)

} // namespace finance::infrastructure::logging