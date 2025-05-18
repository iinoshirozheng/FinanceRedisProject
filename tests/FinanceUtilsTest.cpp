#include <gtest/gtest.h>
#include "utils/FinanceUtils.hpp" // 包含 FinanceUtils 的頭文件
#include <string>
#include <vector>
#include <limits>  // For numeric_limits
#include <cstring> // For strlen

// 使用 FinanceUtils 的命名空間
using namespace finance::utils;

// ============================================================================
// Tests for FinanceUtils::backOfficeToInt
// ============================================================================

TEST(BackOfficeToIntTest, NullOrEmptyInputs)
{
    const char *null_ptr = nullptr;
    char empty_str[] = ""; // For non-null but length 0

    auto res1 = FinanceUtils::backOfficeToInt(null_ptr, 0);
    ASSERT_TRUE(res1.is_err());

    auto res2 = FinanceUtils::backOfficeToInt(empty_str, 0);
    ASSERT_TRUE(res2.is_err());

    // 根據函式實現，value == nullptr 會優先判斷
    auto res3 = FinanceUtils::backOfficeToInt(null_ptr, 5);
    ASSERT_TRUE(res3.is_err());

    char non_empty_str[] = "abc";
    auto res4 = FinanceUtils::backOfficeToInt(non_empty_str, 0); // length is 0
    ASSERT_TRUE(res4.is_err());
}

TEST(BackOfficeToIntTest, AllSpacesInputReturnsOkZero)
{
    // 假設 while (length > 0 && std::isspace...) 修正已加入
    // 修剪後 length 會變為 0，for 循環不執行，返回 Ok(0)
    const char *s1 = "   ";
    auto res1 = FinanceUtils::backOfficeToInt(s1, strlen(s1));
    ASSERT_TRUE(res1.is_ok());
    EXPECT_EQ(res1.unwrap(), 0LL);

    const char *s2 = " ";
    auto res2 = FinanceUtils::backOfficeToInt(s2, strlen(s2));
    ASSERT_TRUE(res2.is_ok());
    EXPECT_EQ(res2.unwrap(), 0LL);
}

TEST(BackOfficeToIntTest, ValidPositiveNumbers)
{
    const char *s1 = "123";
    auto res1 = FinanceUtils::backOfficeToInt(s1, strlen(s1));
    ASSERT_TRUE(res1.is_ok());
    EXPECT_EQ(res1.unwrap(), 123LL);

    const char *s2 = "0";
    auto res2 = FinanceUtils::backOfficeToInt(s2, strlen(s2));
    ASSERT_TRUE(res2.is_ok());
    EXPECT_EQ(res2.unwrap(), 0LL);

    const char *s3 = "9876543210";
    auto res3 = FinanceUtils::backOfficeToInt(s3, strlen(s3));
    ASSERT_TRUE(res3.is_ok());
    EXPECT_EQ(res3.unwrap(), 9876543210LL);
}

TEST(BackOfficeToIntTest, LeadingSpaces)
{
    const char *s1 = "  123";
    auto res1 = FinanceUtils::backOfficeToInt(s1, strlen(s1));
    ASSERT_TRUE(res1.is_ok());
    EXPECT_EQ(res1.unwrap(), 123LL);

    const char *s2 = " J"; // 前導空格，後綴J
    auto res2 = FinanceUtils::backOfficeToInt(s2, strlen(s2));
    ASSERT_TRUE(res2.is_ok());
    EXPECT_EQ(res2.unwrap(), -1LL); // 0*10 + ('J'-'I') = 1 => -1

    const char *s3 = "   }";
    auto res3 = FinanceUtils::backOfficeToInt(s3, strlen(s3));
    ASSERT_TRUE(res3.is_ok());
    EXPECT_EQ(res3.unwrap(), 0LL); // 0*10 => 0 => -0

    const char *s4 = "  12K";
    auto res4 = FinanceUtils::backOfficeToInt(s4, strlen(s4));
    ASSERT_TRUE(res4.is_ok());
    EXPECT_EQ(res4.unwrap(), -122LL); // 12*10 + ('K'-'I'=2) = 122 => -122
}

TEST(BackOfficeToIntTest, TrailingSpaces)
{
    // 假設 while (length > 0 && std::isspace...) 修正已加入
    const char *s1 = "123  ";
    auto res1 = FinanceUtils::backOfficeToInt(s1, strlen(s1));
    ASSERT_TRUE(res1.is_ok());
    EXPECT_EQ(res1.unwrap(), 123LL);

    const char *s2 = "J  ";
    auto res2 = FinanceUtils::backOfficeToInt(s2, strlen(s2));
    ASSERT_TRUE(res2.is_ok());
    EXPECT_EQ(res2.unwrap(), -1LL);

    const char *s3 = "}   ";
    auto res3 = FinanceUtils::backOfficeToInt(s3, strlen(s3));
    ASSERT_TRUE(res3.is_ok());
    EXPECT_EQ(res3.unwrap(), 0LL);

    const char *s4 = "12J  ";
    auto res4 = FinanceUtils::backOfficeToInt(s4, strlen(s4));
    ASSERT_TRUE(res4.is_ok());
    EXPECT_EQ(res4.unwrap(), -121LL);
}

TEST(BackOfficeToIntTest, LeadingAndTrailingSpaces)
{
    const char *s1 = "  123  ";
    auto res1 = FinanceUtils::backOfficeToInt(s1, strlen(s1));
    ASSERT_TRUE(res1.is_ok());
    EXPECT_EQ(res1.unwrap(), 123LL);

    const char *s2 = "  J  ";
    auto res2 = FinanceUtils::backOfficeToInt(s2, strlen(s2));
    ASSERT_TRUE(res2.is_ok());
    EXPECT_EQ(res2.unwrap(), -1LL);
}

TEST(BackOfficeToIntTest, SuffixHandling)
{
    // 'J' (74) - 'I' (73) = 1
    // 'K' (75) - 'I' (73) = 2
    // 'L' (76) - 'I' (73) = 3
    // 'M' (77) - 'I' (73) = 4
    // 'N' (78) - 'I' (73) = 5
    // 'O' (79) - 'I' (73) = 6
    // 'P' (80) - 'I' (73) = 7
    // 'Q' (81) - 'I' (73) = 8
    // 'R' (82) - 'I' (73) = 9
    // '}' -> 0

    EXPECT_EQ(FinanceUtils::backOfficeToInt("J", 1).unwrap(), -1LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("K", 1).unwrap(), -2LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("L", 1).unwrap(), -3LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("M", 1).unwrap(), -4LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("N", 1).unwrap(), -5LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("O", 1).unwrap(), -6LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("P", 1).unwrap(), -7LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("Q", 1).unwrap(), -8LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("R", 1).unwrap(), -9LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("}", 1).unwrap(), 0LL);

    EXPECT_EQ(FinanceUtils::backOfficeToInt("1J", 2).unwrap(), -11LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("12K", 3).unwrap(), -122LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("123L", 4).unwrap(), -1233LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("1234M", 5).unwrap(), -12344LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("1N", 2).unwrap(), -15LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("2O", 2).unwrap(), -26LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("3P", 2).unwrap(), -37LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("4Q", 2).unwrap(), -48LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("5R", 2).unwrap(), -59LL);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("6}", 2).unwrap(), -60LL);
}

TEST(BackOfficeToIntTest, SuffixIgnoresFollowingCharacters)
{
    // 函式的行為是遇到後綴立即返回
    const char *s1 = "12J34"; // 忽略 "34"
    auto res1 = FinanceUtils::backOfficeToInt(s1, strlen(s1));
    ASSERT_TRUE(res1.is_ok());
    EXPECT_EQ(res1.unwrap(), -121LL);

    const char *s2 = "5}67"; // 忽略 "67"
    auto res2 = FinanceUtils::backOfficeToInt(s2, strlen(s2));
    ASSERT_TRUE(res2.is_ok());
    EXPECT_EQ(res2.unwrap(), -50LL);

    const char *s3 = "R1"; // 忽略 "1"
    auto res3 = FinanceUtils::backOfficeToInt(s3, strlen(s3));
    ASSERT_TRUE(res3.is_ok());
    EXPECT_EQ(res3.unwrap(), -9LL);
}

TEST(BackOfficeToIntTest, SpaceInTheMiddleOfNumber)
{
    const char *s1 = "12 3";
    auto res1 = FinanceUtils::backOfficeToInt(s1, strlen(s1));
    ASSERT_TRUE(res1.is_err());

    const char *s2 = "1 2J"; // J 之前，數字之後
    auto res2 = FinanceUtils::backOfficeToInt(s2, strlen(s2));
    ASSERT_TRUE(res2.is_err());

    const char *s3 = "  12 3"; // 前導空格後，數字中間
    auto res3 = FinanceUtils::backOfficeToInt(s3, strlen(s3));
    ASSERT_TRUE(res3.is_err());
}

TEST(BackOfficeToIntTest, InvalidCharacter)
{
    const char *s1 = "12A3"; // A is invalid
    auto res1 = FinanceUtils::backOfficeToInt(s1, strlen(s1));
    ASSERT_TRUE(res1.is_err());
    EXPECT_EQ(res1.unwrap_err().message, "backOfficeToInt: invalid character");

    const char *s2 = "A123"; // A is invalid
    auto res2 = FinanceUtils::backOfficeToInt(s2, strlen(s2));
    ASSERT_TRUE(res2.is_err());
    EXPECT_EQ(res2.unwrap_err().message, "backOfficeToInt: invalid character");

    const char *s3 = "123S"; // S is not a valid suffix
    auto res3 = FinanceUtils::backOfficeToInt(s3, strlen(s3));
    ASSERT_TRUE(res3.is_err());
    EXPECT_EQ(res3.unwrap_err().message, "backOfficeToInt: invalid character");

    const char *s4 = "12{"; // { is not }
    auto res4 = FinanceUtils::backOfficeToInt(s4, strlen(s4));
    ASSERT_TRUE(res4.is_err());
    EXPECT_EQ(res4.unwrap_err().message, "backOfficeToInt: invalid character");

    const char *s5 = "1-2J"; // - is invalid in numeric part by this logic
    auto res5 = FinanceUtils::backOfficeToInt(s5, strlen(s5));
    ASSERT_TRUE(res5.is_err());
    EXPECT_EQ(res5.unwrap_err().message, "backOfficeToInt: invalid character");
}

TEST(BackOfficeToIntTest, NumbersNearLimitsNoOverflowAssumption)
{
    // 由於使用者保證不溢位，我們只測試一些較大的數字
    // 這些數字本身不應該導致此函式（如果無溢位問題）失敗
    const char *s1 = "922337203685477580"; // Max_LL / 10 approx
    auto res1 = FinanceUtils::backOfficeToInt(s1, strlen(s1));
    ASSERT_TRUE(res1.is_ok());
    EXPECT_EQ(res1.unwrap(), 922337203685477580LL);

    const char *s2 = "922337203685477580J"; // (val * 10 + 1)
    auto res2 = FinanceUtils::backOfficeToInt(s2, strlen(s2));
    ASSERT_TRUE(res2.is_ok());
    EXPECT_EQ(res2.unwrap(), -9223372036854775801LL);

    const char *s3 = "922337203685477580}"; // (val * 10)
    auto res3 = FinanceUtils::backOfficeToInt(s3, strlen(s3));
    ASSERT_TRUE(res3.is_ok());
    EXPECT_EQ(res3.unwrap(), -9223372036854775800LL);

    const char *s4 = "0J";
    auto res4 = FinanceUtils::backOfficeToInt(s4, strlen(s4));
    ASSERT_TRUE(res4.is_ok());
    EXPECT_EQ(res4.unwrap(), -1LL);

    const char *s5 = "0}";
    auto res5 = FinanceUtils::backOfficeToInt(s5, strlen(s5));
    ASSERT_TRUE(res5.is_ok());
    EXPECT_EQ(res5.unwrap(), 0LL);
}
// ============================================================================
// Tests for TrimRight
// ============================================================================

TEST(FinanceUtilsTest, TrimRight)
{
    EXPECT_EQ(FinanceUtils::trim_right("test   ", 7), "test");
    EXPECT_EQ(FinanceUtils::trim_right("test", 4), "test");
    EXPECT_EQ(FinanceUtils::trim_right("test\t\n\r ", 8), "test");
    EXPECT_EQ(FinanceUtils::trim_right("   ", 3), "");
    EXPECT_EQ(FinanceUtils::trim_right("", 0), "");
    EXPECT_EQ(FinanceUtils::trim_right(nullptr, 5), "");       // Should handle nullptr
    EXPECT_EQ(FinanceUtils::trim_right(" test ", 6), " test"); // Only trim trailing
    EXPECT_EQ(FinanceUtils::trim_right(" ", 1), "");           // Single space
    EXPECT_EQ(FinanceUtils::trim_right("a", 1), "a");          // No trailing space
    EXPECT_EQ(FinanceUtils::trim_right("a ", 2), "a");         // Single char with trailing space
    EXPECT_EQ(FinanceUtils::trim_right("ab ", 3), "ab");
    EXPECT_EQ(FinanceUtils::trim_right("ab  ", 4), "ab");
}

// ============================================================================
// Tests for TrimRightView
// ============================================================================

TEST(FinanceUtilsTest, TrimRightView_String)
{
    std::string s1 = "test   ";
    std::string s2 = "test";
    std::string s3 = "test\t\n\r ";
    std::string s4 = "   ";
    std::string s5 = "";
    std::string s6 = " test ";
    std::string s7 = " ";
    std::string s8 = "a";
    std::string s9 = "a ";
    std::string s10 = "ab ";
    std::string s11 = "ab  ";

    EXPECT_EQ(FinanceUtils::trim_right_view(s1), "test");
    EXPECT_EQ(FinanceUtils::trim_right_view(s2), "test");
    EXPECT_EQ(FinanceUtils::trim_right_view(s3), "test");
    EXPECT_EQ(FinanceUtils::trim_right_view(s4), "");
    EXPECT_EQ(FinanceUtils::trim_right_view(s5), "");
    EXPECT_EQ(FinanceUtils::trim_right_view(s6), " test");
    EXPECT_EQ(FinanceUtils::trim_right_view(s7), "");  // Single space
    EXPECT_EQ(FinanceUtils::trim_right_view(s8), "a"); // No trailing space
    EXPECT_EQ(FinanceUtils::trim_right_view(s9), "a"); // Single char with trailing space
    EXPECT_EQ(FinanceUtils::trim_right_view(s10), "ab");
    EXPECT_EQ(FinanceUtils::trim_right_view(s11), "ab");
}

TEST(FinanceUtilsTest, TrimRightView_CharPtr)
{
    EXPECT_EQ(FinanceUtils::trim_right_view("test   "), "test");
    EXPECT_EQ(FinanceUtils::trim_right_view("test"), "test");
    EXPECT_EQ(FinanceUtils::trim_right_view("test\t\n\r "), "test");
    EXPECT_EQ(FinanceUtils::trim_right_view("   "), "");
    EXPECT_EQ(FinanceUtils::trim_right_view(""), "");
    EXPECT_EQ(FinanceUtils::trim_right_view(nullptr), "");       // Should handle nullptr
    EXPECT_EQ(FinanceUtils::trim_right_view(" test "), " test"); // Only trim trailing
    EXPECT_EQ(FinanceUtils::trim_right_view(" "), "");           // Single space
    EXPECT_EQ(FinanceUtils::trim_right_view("a"), "a");          // No trailing space
    EXPECT_EQ(FinanceUtils::trim_right_view("a "), "a");         // Single char with trailing space
    EXPECT_EQ(FinanceUtils::trim_right_view("ab "), "ab");
    EXPECT_EQ(FinanceUtils::trim_right_view("ab  "), "ab");
}