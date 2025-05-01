#pragma once
#include <string>
#include <stdexcept>

namespace finance::domain
{
    class Status
    {
    public:
        enum class Code
        {
            OK,
            ERROR,
            CONNECTION_ERROR,
            INITIALIZATION_ERROR,
            DESERIALIZATION_ERROR,
            RUNTIME_ERROR
        };

        Status() : code_(Code::OK) {}
        Status(Code code, const std::string &message) : code_(code), message_(message) {}

        bool isOk() const { return code_ == Code::OK; }
        Code code() const { return code_; }
        const std::string &message() const { return message_; }

        static Status ok() { return Status(); }
        static Status error(Code code, const std::string &message) { return Status(code, message); }

    private:
        Code code_;
        std::string message_;
    };

    class FinanceException : public std::runtime_error
    {
    public:
        explicit FinanceException(const Status &status)
            : std::runtime_error(status.message()), status_(status) {}

        const Status &status() const { return status_; }

    private:
        Status status_;
    };

} // namespace finance::domain