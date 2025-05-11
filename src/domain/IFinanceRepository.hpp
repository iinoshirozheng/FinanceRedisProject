#pragma once

#include <string>
#include <vector>
#include "FinanceDataStructure.hpp"
#include "Result.hpp"

namespace finance::domain
{
    // 金融數據儲存庫介面
    // 負責儲存和檢索金融摘要數據
    template <typename T, typename E>
    class IFinanceRepository
    {
    public:
        /**
         * @brief 虛擬解構函數
         * @details 釋放資源，保證物件安全銷毀
         */
        virtual ~IFinanceRepository() = default;

        /**
         * @brief 初始化儲存庫
         * @return 初始化結果
         */
        virtual Result<void, E> init() = 0;

        /**
         * @brief 載入所有數據
         * @return 載入結果
         */
        virtual Result<void, E> loadAll() = 0;

        /**
         * @brief 獲取數據實體
         * @param key 鍵值，用於標識數據實體
         * @return 若成功儲存返回 true，失敗返回 false
         */
        virtual Result<T, E> get(const std::string &key) = 0;

        /**
         * @brief 儲存數據實體
         * @param key 鍵值，用於標識數據實體
         * @param data 要儲存的數據
         * @return 若成功儲存返回 true，失敗返回 false
         */
        virtual Result<void, E> set(const std::string &key, const T &data) = 0;

        /**
         * @brief 更新數據實體
         * @param key 鍵值，用於標識數據實體
         * @param data 更新後的數據
         * @return 若成功更新返回 true，失敗返回 false
         */
        virtual Result<void, E> update(const std::string &key, const T &data) = 0;

        /**
         * @brief 同步數據到 Redis
         * @param key 鍵值，用於標識數據實體
         * @param data 要同步的數據
         * @return 同步結果
         */
        virtual Result<void, E> sync(const std::string &key, const T &data) = 0;

        /**
         * @brief 刪除數據實體
         * @param key 鍵值，用於標識要刪除的數據實體
         * @return 若成功刪除返回 true，失敗返回 false
         */
        virtual bool remove(const std::string &key) = 0;
    };

} // namespace finance::domain