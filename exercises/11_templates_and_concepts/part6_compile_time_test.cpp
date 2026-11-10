// Part 6: class-type NTTPs (FixedString), CTAD and consteval checks.
#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <type_traits>

#include "compile_time.hpp"

namespace tc = cppref::templates_and_concepts;

TEST(Part6CompileTime, FixedStringViewMatchesLiteral) {
  tc::FixedString s{"/health"};
  EXPECT_EQ(s.view(), std::string_view{"/health"});
}

TEST(Part6CompileTime, RouteParamCountIsComputedAtCompileTime) {
  using Orders = tc::Route<"/users/{id}/orders/{order}">;
  EXPECT_EQ(Orders::param_count, 2u);
  EXPECT_EQ(tc::Route<"/health">::param_count, 0u);
}

TEST(Part6CompileTime, RouteValidityIsAConstantCheck) {
  EXPECT_TRUE(tc::Route<"/users/{id}">::is_valid);
  EXPECT_TRUE(tc::route_is_valid("/users/{id}"));
  EXPECT_FALSE(tc::route_is_valid("/users/{id"));
  EXPECT_FALSE(tc::route_is_valid("users"));
  EXPECT_FALSE(tc::route_is_valid("/users//x"));
}

TEST(Part6CompileTime, RouteMatchesLiteralAndParameterSegments) {
  using R = tc::Route<"/users/{id}">;
  EXPECT_TRUE(R::matches("/users/42"));
  EXPECT_FALSE(R::matches("/orders/42"));
}

TEST(Part6CompileTime, RouteSegmentCountMustMatchExactly) {
  using R = tc::Route<"/users/{id}">;
  EXPECT_TRUE(R::matches("/users/7"));
  EXPECT_FALSE(R::matches("/users"));
  EXPECT_FALSE(R::matches("/users/"));
  EXPECT_FALSE(R::matches("/users/42/extra"));
}

TEST(Part6CompileTime, LabeledCtadDeducesStdStringFromLiteral) {
  tc::Labeled lbl{"hi"};
  EXPECT_TRUE((std::is_same_v<decltype(lbl), tc::Labeled<std::string>>));
  EXPECT_EQ(lbl.value(), "hi");
}

TEST(Part6CompileTime, LabeledCtadDeducesFromValue) {
  tc::Labeled n{42};
  EXPECT_TRUE((std::is_same_v<decltype(n), tc::Labeled<int>>));
  EXPECT_EQ(n.value(), 42);
}
