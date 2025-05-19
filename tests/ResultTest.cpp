
#include "gtest/gtest.h"
#include "domain/Result.hpp"

using namespace finance::domain;

// 測試成功情況
TEST(ResultTest, CreateOkResult)
{
    auto result = Result<int>::Ok(42); // 創建成功結果

    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(result.is_err());
    EXPECT_EQ(result.unwrap(), 42); // 確保值為成功的值
}

// 測試錯誤情況
TEST(ResultTest, CreateErrResult)
{
    auto result = Result<int>::Err(ErrorResult(ErrorCode::RedisKeyNotFound, "Key not found"));

    EXPECT_FALSE(result.is_ok());
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().code, ErrorCode::RedisKeyNotFound); // 確保錯誤代碼
    EXPECT_EQ(result.unwrap_err().message, "Key not found");          // 確保錯誤信息
}

// 測試 unwrap_err 的異常情況
TEST(ResultTest, UnwrapErrOnOkResultThrows)
{
    auto result = Result<int>::Ok(42);                   // 創建成功結果
    EXPECT_THROW(result.unwrap_err(), std::logic_error); // 應拋出異常
}

// 測試 unwrap 的異常情況
TEST(ResultTest, UnwrapOnErrResultThrows)
{
    auto result = Result<int>::Err(ErrorResult(ErrorCode::RedisKeyNotFound, "Key not found"));
    EXPECT_THROW(result.unwrap(), std::logic_error); // 應拋出異常
}

// 測試默認值
TEST(ResultTest, UnwrapOrReturnsDefault)
{
    auto result = Result<int>::Err(ErrorResult(ErrorCode::UnknownTransactionCode, "Invalid transaction"));

    EXPECT_EQ(result.unwrap_or(10), 10); // 如果是錯誤結果，應返回默認值
}

// 測試 map 操作成功情況
TEST(ResultTest, MapTransformsSuccess)
{
    auto result = Result<int>::Ok(42);
    auto mapped_result = result.map([](const int &value) -> double
                                    {
                                        return value * 2.5; // 簡單映射函數
                                    });

    EXPECT_TRUE(mapped_result.is_ok());
    EXPECT_EQ(mapped_result.unwrap(), 105.0); // 確保映射結果
}

// 測試 map 操作錯誤情況
TEST(ResultTest, MapDoesNotTransformError)
{
    auto result = Result<int>::Err(ErrorResult(ErrorCode::TcpStartFailed, "TCP failed"));
    auto mapped_result = result.map([](const int &value) -> double
                                    {
                                        return value * 2.5; // 簡單映射函數
                                    });

    EXPECT_TRUE(mapped_result.is_err());
    EXPECT_EQ(mapped_result.unwrap_err().code, ErrorCode::TcpStartFailed); // 保留錯誤
    EXPECT_EQ(mapped_result.unwrap_err().message, "TCP failed");           // 保留錯誤信息
}

// 測試 map_err 操作錯誤情況
TEST(ResultTest, MapErrTransformsError)
{
    auto result = Result<int>::Err(ErrorResult(ErrorCode::RedisContextAllocationError, "Allocation error"));
    auto mapped_result = result.map_err([](const ErrorResult &err) -> ErrorResult
                                        {
                                            return ErrorResult(ErrorCode::InternalError, "Remapped: " + err.message); // 映射錯誤
                                        });

    EXPECT_TRUE(mapped_result.is_err());
    EXPECT_EQ(mapped_result.unwrap_err().code, ErrorCode::InternalError);        // 確保新的錯誤代碼
    EXPECT_EQ(mapped_result.unwrap_err().message, "Remapped: Allocation error"); // 確保新的錯誤描述
}

// 测试 void 类型的成功情况
TEST(ResultVoidTest, CreateOkResult)
{
    auto result = Result<void, ErrorResult>::Ok();

    EXPECT_TRUE(result.is_ok());
    EXPECT_FALSE(result.is_err());
    EXPECT_NO_THROW(result.unwrap()); // 成功情況下不應拋出異常
}

// 測試 void 類型的錯誤情況
TEST(ResultVoidTest, CreateErrResult)
{
    auto result = Result<void, ErrorResult>::Err(ErrorResult(ErrorCode::JsonParseError, "JSON parsing failed"));

    EXPECT_FALSE(result.is_ok());
    EXPECT_TRUE(result.is_err());
    EXPECT_EQ(result.unwrap_err().code, ErrorCode::JsonParseError); // 確保錯誤代碼
    EXPECT_EQ(result.unwrap_err().message, "JSON parsing failed");  // 確保錯誤信息
}

// 測試 void 類型的 error 映射
TEST(ResultVoidTest, MapErrorForVoidType)
{
    auto result = Result<void, ErrorResult>::Err(ErrorResult(ErrorCode::RedisCommandFailed, "Command failed"));
    auto mapped_result = result.map_err([](const ErrorResult &err) -> ErrorResult
                                        { return ErrorResult(ErrorCode::InternalError, "Remapped: " + err.message); });

    EXPECT_TRUE(mapped_result.is_err());
    EXPECT_EQ(mapped_result.unwrap_err().code, ErrorCode::InternalError);      // 確保新的錯誤代碼
    EXPECT_EQ(mapped_result.unwrap_err().message, "Remapped: Command failed"); // 確保新的錯誤信息
}
