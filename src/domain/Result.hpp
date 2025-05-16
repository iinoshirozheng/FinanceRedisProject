#pragma once

#include <variant>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <optional>

namespace finance::domain
{
    /**
     * @brief 全域錯誤碼定義
     */
    enum class ErrorCode
    {
        Ok = 0,                      // 成功
        RedisInitFailed,             // Redis 初始化失敗
        RedisLoadFailed,             // Redis 載入數據失敗
        RedisCommandFailed,          // Redis 命令執行失敗
        RedisConnectionFailed,       // Redis 連接失敗
        RedisContextAllocationError, // Redis 連接上下文分配錯誤
        RedisReplyTypeError,         // Redis 返回類型錯誤
        RedisKeyNotFound,            // Redis 無法找到指定 Key

        JsonParseError,          // JSON 解析錯誤
        TcpStartFailed,          // TCP 服務啟動失敗
        InvalidPacket,           // 無效封包
        UnknownTransactionCode,  // 未知的交易代碼
        InternalError,           // 內部錯誤
        UnexpectedError,         // 未知錯誤
        BackOfficeIntParseError, // BackOffice 數字解析錯誤
        GetDataNull              // Null ptr
    };

    /**
     * @brief 全域錯誤對象，包含錯誤碼與描述
     */
    struct ErrorResult
    {
        ErrorCode code;      // 錯誤碼
        std::string message; // 錯誤描述

        // Remove noexcept since std::string move constructor may throw
        ErrorResult(ErrorCode c, std::string msg)
            : code(c), message(std::move(msg)) {}
    };

    //
    // T != void 的 Result 類
    //
    template <typename T, typename E = ErrorResult>
    class Result
    {
    public:
        using OkType = T;  // 代表成功的類型
        using ErrType = E; // 代表錯誤的類型

    private:
        std::variant<T, E> value_; // 保存成功值或錯誤值
        bool is_ok_;               // 判斷當前是否為成功狀態

    public:
        /**
         * @brief 構造函數，支持完美轉發
         * @tparam U 構造值類型
         * @param val 成功值或錯誤值（由外部傳入）
         */
        template <typename U>
        explicit constexpr Result(U &&val)
            : value_(std::forward<U>(val)),
              is_ok_(std::is_same<std::decay_t<U>, T>::value) {}

        /**
         * @brief 靜態方法：生成成功結果
         * @tparam U 成功值類型
         * @param val 成功值
         * @return 成功的 Result
         */
        template <typename U>
        static constexpr Result<T, E> Ok(U &&val)
        {
            return Result<T, E>(std::forward<U>(val));
        }

        /**
         * @brief 靜態方法：生成失敗結果
         * @tparam U 錯誤值類型
         * @param err 錯誤值
         * @return 包含錯誤的 Result
         */
        template <typename U>
        static constexpr Result<T, E> Err(U &&err)
        {
            return Result<T, E>(std::forward<U>(err));
        }

        /**
         * @brief 判斷當前結果是否成功
         * @return 如果成功返回 true，否則返回 false
         */
        constexpr bool is_ok() const noexcept { return is_ok_; }

        /**
         * @brief 判斷當前結果是否失敗
         * @return 如果失敗返回 true，否則返回 false
         */
        constexpr bool is_err() const noexcept { return !is_ok_; }

        /**
         * @brief 獲取成功的值（若為錯誤狀態，則拋出異常）
         * @return 成功類型的值
         */
        T &unwrap()
        {
            if (!is_ok_)
                throw std::logic_error("unwrap() called on Err");
            return std::get<T>(value_);
        }

        /**
         * @brief 獲取成功的值（常量版本）
         * @return 成功類型的值
         */
        const T &unwrap() const
        {
            if (!is_ok_)
                throw std::logic_error("unwrap() called on Err");
            return std::get<T>(value_);
        }

        /**
         * @brief 獲取錯誤的值（若為成功狀態，則拋出異常）
         * @return 錯誤類型的值
         */
        E &unwrap_err()
        {
            if (is_ok_)
                throw std::logic_error("unwrap_err() called on Ok");
            return std::get<E>(value_);
        }

        /**
         * @brief 獲取錯誤的值（常量版本）
         * @return 錯誤類型的值
         */
        const E &unwrap_err() const
        {
            if (is_ok_)
                throw std::logic_error("unwrap_err() called on Ok");
            return std::get<E>(value_);
        }

        /**
         * @brief 如果結果是成功，返回成功的值；否則返回默認值
         * @param def 默認值
         * @return 要麼是成功值，要麼返回默認值
         */
        T unwrap_or(T &&def) const
        {
            return is_ok_ ? std::get<T>(value_) : std::forward<T>(def);
        }

        /**
         * @brief 對成功值應用映射函數
         * @tparam U 映射後的類型
         * @param f 映射函數（應用於成功值）
         * @return 映射後的 Result（成功或失敗）
         */
        template <typename Func>
        auto map(Func &&f) const -> Result<std::invoke_result_t<Func, const T &>, E>
        {
            using U = std::invoke_result_t<Func, const T &>;
            if (is_ok_)
                return Result<U, E>::Ok(f(std::get<T>(value_)));
            return Result<U, E>::Err(std::get<E>(value_));
        }

        /**
         * @brief 映射錯誤結果，接受一般 lambda
         * @tparam Func 可呼叫物件類型，返回新錯誤類型
         * @param f 錯誤映射函數
         * @return 錯誤映射後的新 Result
         */
        template <typename Func>
        auto map_err(Func &&f) const
            -> Result<T, std::invoke_result_t<Func, const E &>>
        {
            using NewErr = std::invoke_result_t<Func, const E &>;
            if (is_ok_)
                return Result<T, NewErr>::Ok(std::get<T>(value_));
            return Result<T, NewErr>::Err(std::forward<Func>(f)(std::get<E>(value_)));
        }

        // map_error 為 map_err 的別名
        template <typename Func>
        auto map_error(Func &&f) const
            -> decltype(this->map_err(std::forward<Func>(f)))
        {
            return map_err(std::forward<Func>(f));
        }

        /**
         * @brief 連續進行操作（僅在成功時調用函數）
         * @tparam Func 函數類型，接受 T 並返回 Result<U, E>
         * @param f 操作函數
         * @return 新的 Result
         */
        template <typename Func>
        auto and_then(Func &&f) const -> decltype(f(std::declval<T>()))
        {
            if (is_ok_)
                return f(std::get<T>(value_));

            using ResultType = decltype(f(std::declval<T>()));
            return ResultType::Err(std::get<E>(value_));
        }

        /**
         * @brief 連續進行操作（僅在失敗時調用函數）
         * @tparam Func 函數類型，接受 E 並返回 Result<T, ?>
         * @param f 操作函數
         * @return 新的 Result
         */
        template <typename Func>
        auto or_else(Func &&f) const
            -> decltype(f(std::declval<E>()))
        {
            using ResultType = decltype(f(std::declval<E>()));
            if (is_err())
                return std::forward<Func>(f)(std::get<E>(value_));
            return ResultType::Ok(std::get<T>(value_));
        }

        /**
         * @brief 匹配成功和錯誤兩種情形的行為
         * @tparam OkFn 成功時的處理函數
         * @tparam ErrFn 失敗時的處理函數
         * @param ok_fn 成功時的行為
         * @param err_fn 失敗時的行為
         * @return 匹配後的結果
         */
        template <typename OkFn, typename ErrFn>
        constexpr auto match(OkFn ok_fn, ErrFn err_fn) const -> decltype(ok_fn(std::declval<T>()))
        {
            return is_ok_ ? ok_fn(std::get<T>(value_)) : err_fn(std::get<E>(value_));
        }
    };

    //
    // 特化版本：T = void
    //
    template <typename E>
    class Result<void, E>
    {
        std::optional<E> error_; // 錯誤結果（當有錯誤時設置）

    public:
        constexpr Result() = default;                                // 默認構造函數（成功結果）
        explicit constexpr Result(E err) : error_(std::move(err)) {} // 錯誤結果構造函數

        /**
         * @brief 生成成功結果的靜態方法
         * @return 成功的 Result
         */
        static constexpr Result Ok() noexcept { return Result(); }

        /**
         * @brief 生成失敗結果的靜態方法
         * @return 錯誤的 Result
         */
        static constexpr Result Err(E err) { return Result(std::move(err)); }

        /**
         * @brief 判斷當前是否為成功結果
         * @return 如果成功返回 true
         */
        constexpr bool is_ok() const noexcept { return !error_.has_value(); }

        /**
         * @brief 判斷當前是否為失敗結果
         * @return 如果失敗返回 true
         */
        constexpr bool is_err() const noexcept { return error_.has_value(); }

        /**
         * @brief 獲取成功結果（若為錯誤狀態則拋出異常）
         */
        void unwrap() const
        {
            if (error_.has_value())
                throw std::logic_error("unwrap() called on Err");
        }

        /**
         * @brief 獲取錯誤結果（若為成功狀態則拋出異常）
         * @return 錯誤結果
         */
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

        /**
         * @brief 映射錯誤結果，接受一般 lambda
         * @tparam Func 可呼叫物件類型，返回新錯誤類型
         * @param f 錯誤映射函數
         * @return 映射完成的新 Result
         */
        template <typename Func>
        auto map_err(Func &&f) const
            -> Result<void, std::invoke_result_t<Func, const E &>>
        {
            using NewErr = std::invoke_result_t<Func, const E &>;
            if (is_ok())
                return Result<void, NewErr>::Ok();
            return Result<void, NewErr>::Err(std::forward<Func>(f)(*error_));
        }

        // map_error 為 map_err 的別名
        template <typename Func>
        auto map_error(Func &&f) const
            -> decltype(this->map_err(std::forward<Func>(f)))
        {
            return map_err(std::forward<Func>(f));
        }

        /**
         * @brief 連續操作，僅在成功時執行下一步
         * @tparam Func 函數類型，不接受參數但返回 Result<void, E>
         * @param f 操作函數
         * @return 新的 Result
         */
        template <typename Func>
        auto and_then(Func &&f) const -> decltype(f())
        {
            if (is_ok())
                return f();

            using ResultType = decltype(f());
            return ResultType::Err(*error_);
        }

        /**
         * @brief 連續操作，僅在失敗時執行下一步
         * @tparam Func 函數類型，接受 E 並返回 Result<void, ?>
         * @param f 操作函數
         * @return 新的 Result
         */
        template <typename Func>
        auto or_else(Func &&f) const
            -> decltype(f(std::declval<E>()))
        {
            if (is_err())
                return std::forward<Func>(f)(*error_);
            return decltype(f(std::declval<E>()))::Ok();
        }

        /**
         * @brief 匹配成功和錯誤兩種情形的行為
         */
        template <typename OkFn, typename ErrFn>
        constexpr auto match(OkFn ok_fn, ErrFn err_fn) const -> decltype(ok_fn())
        {
            return is_ok() ? ok_fn() : err_fn(*error_);
        }
    };

} // namespace finance::domain