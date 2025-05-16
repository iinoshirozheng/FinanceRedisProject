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

TEST(FinanceUtilsTest, BackOfficeToInt_BasicNegative)
{
    // Negative integers ('J' to 'R' for 1-9, '}' for 0)
    EXPECT_EQ(FinanceUtils::backOfficeToInt("123J", 4), -1231);              // 123*10 + 1 -> -1231
    EXPECT_EQ(FinanceUtils::backOfficeToInt("45K", 3), -452);                // 45*10 + 2 -> -452
    EXPECT_EQ(FinanceUtils::backOfficeToInt("6R", 2), -69);                  // 6*10 + 9 -> -69
    EXPECT_EQ(FinanceUtils::backOfficeToInt("78}", 3), -780);                // 78*10 + 0 -> -780
    EXPECT_EQ(FinanceUtils::backOfficeToInt("0}", 2), 0);                    // 0*10 + 0 -> 0 (negative zero is still zero)
    EXPECT_EQ(FinanceUtils::backOfficeToInt("J", 1), -1);                    // Assuming implicit 0 before symbol: 0*10 + 1 -> -1
    EXPECT_EQ(FinanceUtils::backOfficeToInt("}", 1), 0);                     // Assuming implicit 0 before symbol: 0*10 + 0 -> 0
    EXPECT_EQ(FinanceUtils::backOfficeToInt("9R", 2), -99);                  // 9*10 + 9 -> -99
    EXPECT_EQ(FinanceUtils::backOfficeToInt("1J", 2), -11);                  // 1*10 + 1 -> -11
    EXPECT_EQ(FinanceUtils::backOfficeToInt("123456789R", 10), -1234567899); // Larger negative number
    EXPECT_EQ(FinanceUtils::backOfficeToInt("123456789}", 10), -1234567890); // Larger negative number ending in '}'
}

TEST(FinanceUtilsTest, BackOfficeToInt_Whitespace)
{
    EXPECT_EQ(FinanceUtils::backOfficeToInt(" 123K", 5), -1232);             // Leading space
    EXPECT_EQ(FinanceUtils::backOfficeToInt("123 ", 4), 1);                  // Trailing space
    EXPECT_EQ(FinanceUtils::backOfficeToInt(" 123 ", 5), 1);                 // Both leading and trailing
    EXPECT_EQ(FinanceUtils::backOfficeToInt("  456}  ", 8), -4560);          // Multiple spaces
    EXPECT_EQ(FinanceUtils::backOfficeToInt("\t\n\r 789 \t\n\r", 11), 1);    // Various whitespaces
    EXPECT_EQ(FinanceUtils::backOfficeToInt("\t\n\r 77R \t\n\r", 11), -779); // Various whitespaces
    EXPECT_EQ(FinanceUtils::backOfficeToInt("1 2 3", 5), 1);                 // Spaces in between digits (should be ignored by current logic)
    EXPECT_EQ(FinanceUtils::backOfficeToInt(" 1 2 J ", 7), 1);               // Mixed spaces and symbol
    EXPECT_EQ(FinanceUtils::backOfficeToInt(" J ", 3), -1);                  // Spaces around symbol
    EXPECT_EQ(FinanceUtils::backOfficeToInt(" R ", 3), -9);                  // Spaces around symbol
    EXPECT_EQ(FinanceUtils::backOfficeToInt("   ", 3), 1);                   // Only spaces
    EXPECT_EQ(FinanceUtils::backOfficeToInt("", 3), 1);                      // Only spaces
    EXPECT_EQ(FinanceUtils::backOfficeToInt("\t\n\r ", 5), 1);               // Only various whitespaces
}

TEST(FinanceUtilsTest, BackOfficeToInt_NullOrEmpty)
{
    EXPECT_EQ(FinanceUtils::backOfficeToInt(nullptr, 0), 1);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("123", 0), 1);   // length is 0, value is ignored
    EXPECT_EQ(FinanceUtils::backOfficeToInt(nullptr, 5), 1); // value is nullptr, length is ignored after check
    EXPECT_EQ(FinanceUtils::backOfficeToInt("", 0), 1);      // empty string, length 0
    char zero_len_buf[] = "";
    EXPECT_EQ(FinanceUtils::backOfficeToInt(zero_len_buf, 0), 1);
    // Testing with a buffer shorter than specified length (UB prone, but testing expected behavior if it doesn't crash)
    char short_buf[] = "1K";                                     // Actual size 3 including null terminator
    EXPECT_EQ(FinanceUtils::backOfficeToInt(short_buf, 2), -12); // Should process "12"
    // EXPECT_EQ(FinanceUtils::backOfficeToInt(short_buf, 5), 12); // UB, might read garbage or crash
}

TEST(FinanceUtilsTest, BackOfficeToInt_IntermediateUnexpectedCharacters)
{
    // Intermediate unexpected characters - should return 0 based on current logic (result reset to 0)
    EXPECT_EQ(FinanceUtils::backOfficeToInt("12A34J", 6), 1); // 'A' in middle -> 0
    EXPECT_EQ(FinanceUtils::backOfficeToInt("56-78}", 6), 1); // '-' in middle -> 0
    EXPECT_EQ(FinanceUtils::backOfficeToInt("abc", 3), 1);    // All invalid -> 0
    EXPECT_EQ(FinanceUtils::backOfficeToInt("12+34R", 6), 1); // '+' in middle -> 0
    EXPECT_EQ(FinanceUtils::backOfficeToInt("1 2A3 ", 6), 1); // Space then invalid char -> 0
    EXPECT_EQ(FinanceUtils::backOfficeToInt(" A123", 5), 1);  // Leading space then invalid char -> 0

    EXPECT_EQ(FinanceUtils::backOfficeToInt("1J23", 4), -11);     // 'J' in middle -> 0
    EXPECT_EQ(FinanceUtils::backOfficeToInt("12}3", 4), -120);    // '}' in middle -> 0
    EXPECT_EQ(FinanceUtils::backOfficeToInt("12}a  d", 4), -120); // '}' in middle -> 0
}

TEST(FinanceUtilsTest, BackOfficeToInt_LeadingUnexpectedCharacters)
{
    // Unexpected character at the very beginning - should return 0 based on current logic
    EXPECT_EQ(FinanceUtils::backOfficeToInt("A123", 4), 1);  // 'A' at beginning -> 0
    EXPECT_EQ(FinanceUtils::backOfficeToInt("-123J", 5), 1); // '-' at beginning -> 0
    EXPECT_EQ(FinanceUtils::backOfficeToInt("+123J", 5), 1); // '+' at beginning -> 0
}

TEST(FinanceUtilsTest, BackOfficeToInt_TrailingUnexpectedCharacters)
{
    // Unexpected character at the end (not 'J'-'R' or '}' or space) - should return parsed numeric part
    EXPECT_EQ(FinanceUtils::backOfficeToInt("123X", 4), 1);      // 'X' at end -> 123
    EXPECT_EQ(FinanceUtils::backOfficeToInt("45!", 3), 1);       // '!' at end -> 45
    EXPECT_EQ(FinanceUtils::backOfficeToInt("123J.", 5), -1231); // '.' after symbol -> parsing stops at '.' symbol is processed
    EXPECT_EQ(FinanceUtils::backOfficeToInt("78}a", 4), -780);   // 'a' after symbol -> parsing stops at 'a', symbol is processed
    EXPECT_EQ(FinanceUtils::backOfficeToInt("R.", 2), -9);       // 'R' then '.', stops at '.' -> -9
    EXPECT_EQ(FinanceUtils::backOfficeToInt("}. ", 3), 0);       // '}', then '.', then space -> 0
}

TEST(FinanceUtilsTest, BackOfficeToInt_MixedValidInvalid)
{
    // Mixed valid and invalid characters - check expected behavior based on where invalid char is
    // Based on current logic: first unexpected non-space char stops parsing.
    EXPECT_EQ(FinanceUtils::backOfficeToInt("12Kx", 4), -122); // 'K' in middle -> 0
    EXPECT_EQ(FinanceUtils::backOfficeToInt("1A2J", 4), 1);    // 'A' in middle -> 0
    EXPECT_EQ(FinanceUtils::backOfficeToInt("12}3", 4), -120); // '}' in middle -> 0
    EXPECT_EQ(FinanceUtils::backOfficeToInt("1 2A3 ", 6), 1);  // Space is skipped, 'A' stops parsing -> 0
}

TEST(FinanceUtilsTest, BackOfficeToInt_LengthParameterEffect)
{
    // Ensure the 'length' parameter correctly limits processing
    EXPECT_EQ(FinanceUtils::backOfficeToInt("12345", 3), 1);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("123J5", 4), -1231);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("123 ", 3), 1);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("12R ", 3), -129);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("12345", 5), 1);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("123J", 4), -1231);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("12", 5), 1);

    char buffer_with_more_data[] = "1234N67890";
    EXPECT_EQ(FinanceUtils::backOfficeToInt(buffer_with_more_data, 5), -12345);
    char buffer_with_symbol[] = "123Jabc";
    EXPECT_EQ(FinanceUtils::backOfficeToInt(buffer_with_symbol, 3), 1);
    EXPECT_EQ(FinanceUtils::backOfficeToInt(buffer_with_symbol, 4), -1231);
    EXPECT_EQ(FinanceUtils::backOfficeToInt(buffer_with_symbol, 5), -1231);
}

TEST(FinanceUtilsTest, BackOfficeToInt_LargeValues)
{
    // Test with values near int64_t limits (though backend format limits might be smaller)
    // Based on margin_amount[11] -> max 10 digits + symbol
    EXPECT_EQ(FinanceUtils::backOfficeToInt("9999999999R", 11), -99999999999); // 10 nines + R
    EXPECT_EQ(FinanceUtils::backOfficeToInt("9999999999}", 11), -99999999990); // 10 nines + }
    EXPECT_EQ(FinanceUtils::backOfficeToInt("99999999999", 11), 1);            // 11 nines (if allowed)
    // Based on margin_qty[6] -> max 5 digits + symbol
    EXPECT_EQ(FinanceUtils::backOfficeToInt("99999R", 6), -999999); // 5 nines + R
    EXPECT_EQ(FinanceUtils::backOfficeToInt("99999}", 6), -999990); // 5 nines + }
    EXPECT_EQ(FinanceUtils::backOfficeToInt("99999", 5), 1);        // 5 nines
    EXPECT_EQ(FinanceUtils::backOfficeToInt("999999", 6), 1);       // 6 nines (if allowed)
}

TEST(FinanceUtilsTest, BackOfficeToInt_SymbolOnly)
{
    // Test with just the symbol, should be 0 + value
    EXPECT_EQ(FinanceUtils::backOfficeToInt("J", 1), -1);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("R", 1), -9);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("}", 1), 0);
    EXPECT_EQ(FinanceUtils::backOfficeToInt(" ", 1), 1);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("j", 1), 1);
    EXPECT_EQ(FinanceUtils::backOfficeToInt("a", 1), 1);
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