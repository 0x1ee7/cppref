#include "adl.hpp"

#include <gtest/gtest.h>

#include <string>

using shapes::Point;

// --- Part4a: ADL customisation points, niebloids and constrained templates ---

TEST(Part4Adl, SwapValuesUsesTypeSwapViaAdl) {
    Point a{1, 2};
    Point b{3, 4};
    adl::swap_values(a, b);
    EXPECT_EQ(a.x, 3);
    EXPECT_EQ(a.y, 4);
    EXPECT_EQ(b.x, 1);
    EXPECT_EQ(b.y, 2);
}

TEST(Part4Adl, SwapValuesFallsBackToStdSwap) {
    std::string a = "x";
    std::string b = "y";
    adl::swap_values(a, b);
    EXPECT_EQ(a, "y");
    EXPECT_EQ(b, "x");
}

TEST(Part4Adl, ShapesToStringFoundByAdl) {
    EXPECT_EQ(to_string(Point{1, 2}), "(1, 2)");
}

TEST(Part4Adl, TextOfUsesConstrainedToString) {
    Point p{1, 2};
    EXPECT_EQ(adl::text_of(p), "(1, 2)");
    EXPECT_TRUE(adl::Textual<Point>);
    EXPECT_FALSE(adl::Textual<int>);
}
