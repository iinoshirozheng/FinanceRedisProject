#pragma once
#include <string>
#include <sstream>

namespace finance::domain
{
    /**
     * @brief 用於處理操作狀態和錯誤報告的類別，包含上下文資訊。
     *
     * 此類別提供了一個標準化的方式來報告應用程式中的操作狀態和錯誤。
     * 支援鏈式設定上下文資訊，包括操作詳情、鍵值、請求和回應。
     * 狀態資訊可用於日誌記錄、錯誤處理和除錯。
     */
    class Status
    {
    public:
        /**
         * @brief 不同類型的操作和錯誤的狀態碼。
         *
         * 這些代碼代表系統中操作的可能結果：
         * - OK: 操作成功完成
         * - NotFound: 找不到請求的資源
         * - ParseError: 解析輸入資料時發生錯誤
         * - RedisError: Redis 操作錯誤
         * - IOError: 輸入/輸出操作錯誤
         * - ConnectionError: 網路或服務連接錯誤
         * - DeserializationError: 資料反序列化錯誤
         * - InitializationError: 系統初始化錯誤
         * - RuntimeError: 一般執行時期錯誤
         * - Unknown: 未指定的錯誤類型
         */
        enum class Code
        {
            OK,
            NotFound,
            ParseError,
            RedisError,
            IOError,
            ConnectionError,
            DeserializationError,
            InitializationError,
            RuntimeError,
            Unknown,
            ValidationError
        };

        /**
         * @brief 使用指定的代碼和訊息建構 Status 物件。
         *
         * @param code 狀態碼（預設為 OK）
         * @param message 描述狀態的訊息（預設為空字串）
         */
        Status(Code code = Code::OK, std::string message = "")
            : code_(code), message_(std::move(message)) {}

        /**
         * @brief 建立表示成功操作的 Status 物件。
         *
         * @return 狀態碼為 Code::OK 的 Status 物件
         */
        static Status ok() { return Status(Code::OK, ""); }

        /**
         * @brief 建立表示錯誤的 Status 物件。
         *
         * @param c 錯誤代碼
         * @param msg 描述錯誤的訊息
         * @return 具有指定錯誤代碼和訊息的 Status 物件
         */
        static Status error(Code c, const std::string &msg) { return Status(c, msg); }

        /**
         * @brief 設定此狀態的操作類型。
         *
         * @param op 操作的名稱/類型
         * @return 此 Status 物件的參考，用於鏈式呼叫
         */
        Status &withOperation(const std::string &op)
        {
            operation_ = op;
            return *this;
        }

        /**
         * @brief 設定與此狀態相關的鍵值。
         *
         * @param key 鍵值（例如：Redis 鍵值、檔案路徑）
         * @return 此 Status 物件的參考，用於鏈式呼叫
         */
        Status &withKey(const std::string &key)
        {
            key_ = key;
            return *this;
        }

        /**
         * @brief 設定此狀態的請求資料。
         *
         * @param req 請求資料或參數
         * @return 此 Status 物件的參考，用於鏈式呼叫
         */
        Status &withRequest(const std::string &req)
        {
            request_ = req;
            return *this;
        }

        /**
         * @brief 設定此狀態的回應資料。
         *
         * @param resp 回應資料
         * @return 此 Status 物件的參考，用於鏈式呼叫
         */
        Status &withResponse(const std::string &resp)
        {
            response_ = resp;
            return *this;
        }

        /**
         * @brief 檢查狀態是否表示操作成功。
         *
         * @return 如果狀態碼為 OK 則返回 true，否則返回 false
         */
        bool isOk() const { return code_ == Code::OK; }

        /**
         * @brief 獲取狀態碼。
         *
         * @return 目前的狀態碼
         */
        Code code() const { return code_; }

        /**
         * @brief 獲取狀態訊息。
         *
         * @return 目前的狀態訊息
         */
        const std::string &message() const { return message_; }

        /**
         * @brief 將狀態轉換為字串表示。
         *
         * 字串格式包含：
         * - 狀態碼
         * - 訊息（如果有）
         * - 操作類型（如果有）
         * - 鍵值（如果有）
         * - 請求資料（如果有）
         * - 回應資料（如果有）
         *
         * @return 狀態的字串表示
         */
        std::string toString() const
        {
            std::ostringstream oss;
            oss << "Status[" << codeToString(code_) << "]";
            if (!message_.empty())
                oss << " msg=\"" << message_ << "\"";
            if (!operation_.empty())
                oss << " op=\"" << operation_ << "\"";
            if (!key_.empty())
                oss << " key=\"" << key_ << "\"";
            if (!request_.empty())
                oss << " req=" << request_;
            if (!response_.empty())
                oss << " resp=" << response_;
            return oss.str();
        }

    private:
        /**
         * @brief 將狀態碼轉換為其字串表示。
         *
         * @param c 要轉換的狀態碼
         * @return 狀態碼的字串表示
         */
        static const char *codeToString(Code c)
        {
            switch (c)
            {
            case Code::OK:
                return "OK";
            case Code::NotFound:
                return "NotFound";
            case Code::ParseError:
                return "ParseError";
            case Code::RedisError:
                return "RedisError";
            case Code::IOError:
                return "IOError";
            case Code::ConnectionError:
                return "ConnectionError";
            case Code::DeserializationError:
                return "DeserializationError";
            case Code::InitializationError:
                return "InitializationError";
            case Code::RuntimeError:
                return "RuntimeError";
            case Code::ValidationError:
                return "ValidationError";
            default:
                return "Unknown";
            }
        }

        Code code_;           ///< 狀態碼
        std::string message_; ///< 狀態訊息

        // 上下文欄位
        std::string operation_; ///< 操作類型/名稱
        std::string key_;       ///< 相關的鍵值
        std::string request_;   ///< 請求資料
        std::string response_;  ///< 回應資料
    };

} // namespace finance::domain