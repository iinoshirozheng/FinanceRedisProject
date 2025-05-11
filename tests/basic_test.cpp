#include <gtest/gtest.h>

// Basic test case
TEST(BasicTest, AssertTrue)
{
    EXPECT_TRUE(true);
}

TEST(BasicTest, AssertEqual)
{
    EXPECT_EQ(2 + 2, 4);
}

// Main function (optional, as gtest_main will be linked)
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}