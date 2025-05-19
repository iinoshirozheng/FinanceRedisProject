
#include <gtest/gtest.h>
#include "domain/FinanceDataStructure.hpp"

// Make the namespace accessible for brevity in tests
using namespace finance::domain;

// Test fixture for SummaryData tests
class SummaryDataTest : public ::testing::Test
{
protected:
    SummaryData summary; // Test object

    // SetUp is called before each test
    void SetUp() override
    {
        // Ensure 'summary' is in a known default state before each test
        summary = SummaryData();
    }

    // TearDown can be used for cleanup after each test if needed
    // void TearDown() override {}
};

// Test Case: Default Constructor Initialization
TEST_F(SummaryDataTest, DefaultInitialization)
{
    EXPECT_EQ(summary.margin_available_amount, 0);
    EXPECT_EQ(summary.margin_available_qty, 0);
    EXPECT_EQ(summary.short_available_amount, 0);
    EXPECT_EQ(summary.short_available_qty, 0);
    EXPECT_EQ(summary.after_margin_available_amount, 0);
    EXPECT_EQ(summary.after_margin_available_qty, 0);
    EXPECT_EQ(summary.after_short_available_amount, 0);
    EXPECT_EQ(summary.after_short_available_qty, 0);

    EXPECT_TRUE(summary.stock_id.empty());
    EXPECT_TRUE(summary.area_center.empty());
    EXPECT_TRUE(summary.belong_branches.empty());

    // Check raw HCRTM01 data fields
    EXPECT_EQ(summary.h01_margin_amount, 0);
    EXPECT_EQ(summary.h01_margin_buy_order_amount, 0);
    EXPECT_EQ(summary.h01_margin_sell_match_amount, 0);
    EXPECT_EQ(summary.h01_margin_qty, 0);
    EXPECT_EQ(summary.h01_margin_buy_order_qty, 0);
    EXPECT_EQ(summary.h01_margin_sell_match_qty, 0);
    EXPECT_EQ(summary.h01_short_amount, 0);
    EXPECT_EQ(summary.h01_short_sell_order_amount, 0);
    EXPECT_EQ(summary.h01_short_qty, 0);
    EXPECT_EQ(summary.h01_short_sell_order_qty, 0);
    EXPECT_EQ(summary.h01_short_after_hour_sell_order_amount, 0);
    EXPECT_EQ(summary.h01_short_after_hour_sell_order_qty, 0);
    EXPECT_EQ(summary.h01_short_sell_match_amount, 0);
    EXPECT_EQ(summary.h01_short_sell_match_qty, 0);
    EXPECT_EQ(summary.h01_margin_after_hour_buy_order_amount, 0);
    EXPECT_EQ(summary.h01_margin_after_hour_buy_order_qty, 0);
    EXPECT_EQ(summary.h01_margin_buy_match_amount, 0);
    EXPECT_EQ(summary.h01_margin_buy_match_qty, 0);

    // Check raw HCRTM05P data fields
    EXPECT_EQ(summary.h05p_margin_buy_offset_qty, 0);
    EXPECT_EQ(summary.h05p_short_sell_offset_qty, 0);
}

// Test Case: CalculateAvailables with All Zero Inputs
TEST_F(SummaryDataTest, CalculateAvailables_AllZeroInputs)
{
    // All h01_ and h05p_ fields are already 0 due to default initialization
    // and SetUp() re-initializing 'summary'.
    // For explicitness, one could set them to 0 here again, but it's not strictly needed.

    summary.calculate_availables();

    EXPECT_EQ(summary.margin_available_amount, 0);
    EXPECT_EQ(summary.margin_available_qty, 0);
    EXPECT_EQ(summary.short_available_amount, 0);
    EXPECT_EQ(summary.short_available_qty, 0);
    EXPECT_EQ(summary.after_margin_available_amount, 0);
    EXPECT_EQ(summary.after_margin_available_qty, 0);
    EXPECT_EQ(summary.after_short_available_amount, 0);
    EXPECT_EQ(summary.after_short_available_qty, 0);
}

// Test Case: CalculateAvailables with Positive Margin Values
TEST_F(SummaryDataTest, CalculateAvailables_PositiveMarginValues)
{
    summary.h01_margin_amount = 1000000;
    summary.h01_margin_buy_order_amount = 200000;
    summary.h01_margin_sell_match_amount = 50000;
    summary.h01_margin_qty = 100;
    summary.h01_margin_buy_order_qty = 20;
    summary.h01_margin_sell_match_qty = 5;
    summary.h05p_margin_buy_offset_qty = 10; // From HCRTM05P

    summary.h01_margin_buy_match_amount = 150000;
    summary.h01_margin_buy_match_qty = 15;
    summary.h01_margin_after_hour_buy_order_amount = 30000;
    summary.h01_margin_after_hour_buy_order_qty = 3;

    summary.calculate_availables();

    EXPECT_EQ(summary.margin_available_amount, 1000000 - 200000 + 50000); // 850000
    EXPECT_EQ(summary.margin_available_qty, 100 - 20 + 5 + 10);           // 95

    EXPECT_EQ(summary.after_margin_available_amount, 1000000 - 150000 + 50000 - 30000); // 870000
    EXPECT_EQ(summary.after_margin_available_qty, 100 - 15 + 5 - 3 + 10);               // 97

    // Short related available fields should remain 0 as their inputs are 0
    EXPECT_EQ(summary.short_available_amount, 0);
    EXPECT_EQ(summary.short_available_qty, 0);
    EXPECT_EQ(summary.after_short_available_amount, 0);
    EXPECT_EQ(summary.after_short_available_qty, 0);
}

// Test Case: CalculateAvailables with Positive Short Selling Values
TEST_F(SummaryDataTest, CalculateAvailables_PositiveShortValues)
{
    summary.h01_short_amount = 500000;
    summary.h01_short_sell_order_amount = 100000;
    summary.h01_short_qty = 50;
    summary.h01_short_sell_order_qty = 10;
    summary.h05p_short_sell_offset_qty = 5; // From HCRTM05P

    summary.h01_short_sell_match_amount = 80000;
    // summary.h01_short_sell_match_qty = 8; // Not used in after_short_available_qty calculation directly
    summary.h01_short_after_hour_sell_order_amount = 20000;
    summary.h01_short_after_hour_sell_order_qty = 2;

    summary.calculate_availables();

    EXPECT_EQ(summary.short_available_amount, 500000 - 100000); // 400000
    EXPECT_EQ(summary.short_available_qty, 50 - 10 + 5);        // 45

    EXPECT_EQ(summary.after_short_available_amount, 500000 - 80000 - 20000); // 400000
    EXPECT_EQ(summary.after_short_available_qty, 50 - 10 - 2 + 5);           // 43

    // Margin related available fields should remain 0
    EXPECT_EQ(summary.margin_available_amount, 0);
    EXPECT_EQ(summary.margin_available_qty, 0);
    EXPECT_EQ(summary.after_margin_available_amount, 0);
    EXPECT_EQ(summary.after_margin_available_qty, 0);
}

// Test Case: CalculateAvailables with Mixed Positive Values for Margin and Short
TEST_F(SummaryDataTest, CalculateAvailables_MixedPositiveValues)
{
    // Margin Inputs
    summary.h01_margin_amount = 2000000;
    summary.h01_margin_buy_order_amount = 300000;
    summary.h01_margin_sell_match_amount = 100000;
    summary.h01_margin_qty = 200;
    summary.h01_margin_buy_order_qty = 30;
    summary.h01_margin_sell_match_qty = 10;
    summary.h05p_margin_buy_offset_qty = 15;
    summary.h01_margin_buy_match_amount = 250000;
    summary.h01_margin_buy_match_qty = 25;
    summary.h01_margin_after_hour_buy_order_amount = 50000;
    summary.h01_margin_after_hour_buy_order_qty = 5;

    // Short Inputs
    summary.h01_short_amount = 800000;
    summary.h01_short_sell_order_amount = 150000;
    summary.h01_short_qty = 80;
    summary.h01_short_sell_order_qty = 15;
    summary.h05p_short_sell_offset_qty = 7;
    summary.h01_short_sell_match_amount = 120000;
    // summary.h01_short_sell_match_qty = 12; // Not used in after_short_available_qty
    summary.h01_short_after_hour_sell_order_amount = 40000;
    summary.h01_short_after_hour_sell_order_qty = 4;

    summary.calculate_availables();

    // Expected Margin Values
    EXPECT_EQ(summary.margin_available_amount, 2000000 - 300000 + 100000);               // 1800000
    EXPECT_EQ(summary.margin_available_qty, 200 - 30 + 10 + 15);                         // 195
    EXPECT_EQ(summary.after_margin_available_amount, 2000000 - 250000 + 100000 - 50000); // 1800000
    EXPECT_EQ(summary.after_margin_available_qty, 200 - 25 + 10 - 5 + 15);               // 195

    // Expected Short Values
    EXPECT_EQ(summary.short_available_amount, 800000 - 150000);               // 650000
    EXPECT_EQ(summary.short_available_qty, 80 - 15 + 7);                      // 72
    EXPECT_EQ(summary.after_short_available_amount, 800000 - 120000 - 40000); // 640000
    EXPECT_EQ(summary.after_short_available_qty, 80 - 15 - 4 + 7);            // 68
}

// Test Case: Ensures raw input data is not modified by the calculation
TEST_F(SummaryDataTest, CalculateAvailables_RawDataUnchanged)
{
    summary.h01_margin_amount = 12345;
    summary.h01_margin_qty = 123;
    summary.h05p_margin_buy_offset_qty = 45;
    summary.h01_short_amount = 67890;
    summary.h01_short_qty = 67;
    summary.h05p_short_sell_offset_qty = 89;

    // Store initial values
    int64_t initial_h01_margin_amount = summary.h01_margin_amount;
    int64_t initial_h01_margin_qty = summary.h01_margin_qty;
    int64_t initial_h05p_margin_buy_offset_qty = summary.h05p_margin_buy_offset_qty;
    int64_t initial_h01_short_amount = summary.h01_short_amount;
    int64_t initial_h01_short_qty = summary.h01_short_qty;
    int64_t initial_h05p_short_sell_offset_qty = summary.h05p_short_sell_offset_qty;

    summary.calculate_availables(); // Perform calculation

    // Verify raw data fields remain unchanged
    EXPECT_EQ(summary.h01_margin_amount, initial_h01_margin_amount);
    EXPECT_EQ(summary.h01_margin_qty, initial_h01_margin_qty);
    // ... (add all other h01_ and h05p_ fields that are inputs to the calculation)
    EXPECT_EQ(summary.h01_margin_buy_order_amount, 0); // Was 0 initially, should stay 0 if not set
    EXPECT_EQ(summary.h05p_margin_buy_offset_qty, initial_h05p_margin_buy_offset_qty);

    EXPECT_EQ(summary.h01_short_amount, initial_h01_short_amount);
    EXPECT_EQ(summary.h01_short_qty, initial_h01_short_qty);
    EXPECT_EQ(summary.h05p_short_sell_offset_qty, initial_h05p_short_sell_offset_qty);
}

// Test Case: String and Vector Members (simple check of usability)
TEST_F(SummaryDataTest, StringAndVectorMembers)
{
    summary.stock_id = "0050";
    summary.area_center = "01";
    summary.belong_branches.push_back("BranchX");
    summary.belong_branches.push_back("BranchY");

    EXPECT_EQ(summary.stock_id, "0050");
    EXPECT_EQ(summary.area_center, "01");
    ASSERT_EQ(summary.belong_branches.size(), 2); // Use ASSERT if subsequent checks depend on this
    EXPECT_EQ(summary.belong_branches[0], "BranchX");
    EXPECT_EQ(summary.belong_branches[1], "BranchY");
}
