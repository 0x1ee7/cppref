// Part 5: units of measure with compile-time dimension checks.
#include <gtest/gtest.h>

#include <type_traits>

#include "units.hpp"

namespace tc = cppref::templates_and_concepts;

TEST(Part5Units, SameDimensionAddition) {
  EXPECT_DOUBLE_EQ((tc::meters(3) + tc::meters(4)).value(), 7.0);
}

TEST(Part5Units, SubtractionAndOrdering) {
  EXPECT_DOUBLE_EQ((tc::meters(5) - tc::meters(2)).value(), 3.0);
  EXPECT_TRUE(tc::meters(2) < tc::meters(3));
  EXPECT_TRUE(tc::meters(2) == tc::meters(2));
}

TEST(Part5Units, ScalarMultiplicationScales) {
  EXPECT_DOUBLE_EQ((tc::meters(2) * 3.0).value(), 6.0);
  EXPECT_DOUBLE_EQ((tc::seconds(1.5) * 2.0).value(), 3.0);
}

TEST(Part5Units, ProductCombinesDimensions) {
  auto area = tc::meters(3) * tc::meters(4);
  EXPECT_TRUE((std::is_same_v<decltype(area), tc::SquareMeters>));
  EXPECT_DOUBLE_EQ(area.value(), 12.0);
}

TEST(Part5Units, QuotientYieldsSpeed) {
  auto speed = tc::meters(10) / tc::seconds(2);
  EXPECT_TRUE((std::is_same_v<decltype(speed), tc::MetersPerSecond>));
  EXPECT_DOUBLE_EQ(speed.value(), 5.0);
}

TEST(Part5Units, SameDimensionQuotientIsDimensionless) {
  auto ratio = tc::meters(6) / tc::meters(3);
  EXPECT_TRUE((std::is_same_v<decltype(ratio), tc::Quantity<tc::Dim<0, 0>>>));
  EXPECT_DOUBLE_EQ(ratio.value(), 2.0);
}

TEST(Part5Units, AdditionAcrossDimensionsIsRejected) {
  EXPECT_DOUBLE_EQ((tc::meters(1) + tc::meters(1)).value(), 2.0);
  EXPECT_TRUE((tc::CanAdd<tc::LengthDim, tc::LengthDim>));
  EXPECT_FALSE((tc::CanAdd<tc::LengthDim, tc::TimeDim>));
}
