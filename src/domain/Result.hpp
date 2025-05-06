#pragma once

#include <variant>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include "Error.hpp"

namespace finance::domain
{

    struct ResultError
    {
        std::string message;
        explicit ResultError(const std::string &msg) : message(msg) {}
        friend std::ostream &operator<<(std::ostream &os, const ResultError &err)
        {
            return os << "ResultError: " << err.message;
        }
    };

    //
    // T != void
    //
    template <typename T, typename E = Error>
    class Result
    {
    public:
        using OkType = T;
        using ErrType = E;

    private:
        std::variant<T, E> value_;
        bool is_ok_;

    public:
        Result(const T &val) : value_(val), is_ok_(true) {}
        Result(T &&val) : value_(std::move(val)), is_ok_(true) {}
        Result(const E &err) : value_(err), is_ok_(false) {}
        Result(E &&err) : value_(std::move(err)), is_ok_(false) {}

        static Result Ok(T val) { return Result(std::move(val)); }
        static Result Err(E err) { return Result(std::move(err)); }

        bool is_ok() const { return is_ok_; }
        bool is_err() const { return !is_ok_; }

        T &unwrap()
        {
            if (!is_ok_)
                throw std::logic_error("unwrap() called on Err");
            return std::get<T>(value_);
        }

        const T &unwrap() const
        {
            if (!is_ok_)
                throw std::logic_error("unwrap() called on Err");
            return std::get<T>(value_);
        }

        E &unwrap_err()
        {
            if (is_ok_)
                throw std::logic_error("unwrap_err() called on Ok");
            return std::get<E>(value_);
        }

        const E &unwrap_err() const
        {
            if (is_ok_)
                throw std::logic_error("unwrap_err() called on Ok");
            return std::get<E>(value_);
        }

        T unwrap_or(const T &def) const
        {
            return is_ok_ ? std::get<T>(value_) : def;
        }

        template <typename U>
        Result<U, E> map(std::function<U(const T &)> f) const
        {
            if (is_ok_)
                return Result<U, E>::Ok(f(std::get<T>(value_)));
            return Result<U, E>::Err(std::get<E>(value_));
        }

        template <typename F>
        Result<T, F> map_err(std::function<F(const E &)> f) const
        {
            if (is_ok_)
                return Result<T, F>::Ok(std::get<T>(value_));
            return Result<T, F>::Err(f(std::get<E>(value_)));
        }

        template <typename U>
        Result<U, E> and_then(std::function<Result<U, E>(const T &)> f) const
        {
            if (is_ok_)
                return f(std::get<T>(value_));
            return Result<U, E>::Err(std::get<E>(value_));
        }

        template <typename OkFn, typename ErrFn>
        auto match(OkFn ok_fn, ErrFn err_fn) const -> decltype(ok_fn(std::declval<T>()))
        {
            return is_ok_ ? ok_fn(std::get<T>(value_)) : err_fn(std::get<E>(value_));
        }
    };

    //
    // T = void（專門版本）
    //
    template <typename E>
    class Result<void, E>
    {
        std::optional<E> error_;

    public:
        Result() = default;
        explicit Result(E err) : error_(std::move(err)) {}

        static Result Ok() { return Result(); }
        static Result Err(E err) { return Result(std::move(err)); }

        bool is_ok() const { return !error_.has_value(); }
        bool is_err() const { return error_.has_value(); }

        void unwrap() const
        {
            if (error_.has_value())
                throw std::logic_error("unwrap() called on Err");
        }

        E &unwrap_err()
        {
            if (!error_.has_value())
                throw std::logic_error("unwrap_err() called on Ok");
            return *error_;
        }

        const E &unwrap_err() const
        {
            if (!error_.has_value())
                throw std::logic_error("unwrap_err() called on Ok");
            return *error_;
        }

        template <typename F>
        Result<void, F> map_err(std::function<F(const E &)> f) const
        {
            if (is_ok())
                return Result<void, F>::Ok();
            return Result<void, F>::Err(f(*error_));
        }

        Result<void, E> and_then(std::function<Result<void, E>()> f) const
        {
            if (is_ok())
                return f();
            return *this;
        }

        template <typename OkFn, typename ErrFn>
        auto match(OkFn ok_fn, ErrFn err_fn) const -> decltype(ok_fn())
        {
            return is_ok() ? ok_fn() : err_fn(*error_);
        }
    };

} // namespace finance::domain
