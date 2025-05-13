#include <gtest/gtest.h>
#include <optional>
#include <variant>
#include <string>

// Basic test case
TEST(BasicTest, AssertTrue)
{
    EXPECT_TRUE(true);
}

TEST(BasicTest, AssertEqual)
{
    EXPECT_EQ(2 + 2, 4);
}

// Test C++17 features
TEST(BasicTest, StdOptional)
{
    std::optional<int> opt = 42;
    EXPECT_TRUE(opt.has_value());
    EXPECT_EQ(*opt, 42);
}

TEST(BasicTest, StdVariant)
{
    std::variant<int, std::string> var = 123;
    EXPECT_TRUE(std::holds_alternative<int>(var));
    EXPECT_EQ(std::get<int>(var), 123);

    var = "test";
    EXPECT_TRUE(std::holds_alternative<std::string>(var));
    EXPECT_EQ(std::get<std::string>(var), "test");
}

