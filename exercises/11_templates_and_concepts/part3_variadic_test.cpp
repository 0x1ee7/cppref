// Part 3: variadic templates, fold expressions, if constexpr, type traits and
// std::type_identity.
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "variadic.hpp"

namespace tc = cppref::templates_and_concepts;

TEST(Part3Variadic, SumAllUsesCommonTypeOfArguments) {
  EXPECT_EQ(tc::sum_all(1, 2, 3), 6);
  EXPECT_EQ(tc::sum_all(std::uint8_t{20}, std::uint8_t{10}), 30);
  EXPECT_TRUE((std::is_same_v<
      decltype(tc::sum_all(std::uint8_t{1}, std::uint8_t{2})), std::uint8_t>));
}

TEST(Part3Variadic, SumAllMixedWidthsPromoteToWiderType) {
  auto total = tc::sum_all(std::int64_t{5}, 2);
  EXPECT_EQ(total, 7);
  EXPECT_TRUE((std::is_same_v<decltype(total), std::int64_t>));
}

TEST(Part3Variadic, CountOfCountsOccurrencesOfType) {
  EXPECT_EQ((tc::count_of_v<int, int, long, int>), 2u);
  EXPECT_EQ((tc::count_of_v<std::string, int, std::string>), 1u);
  EXPECT_EQ((tc::count_of_v<char, int, long>), 0u);
}

TEST(Part3Variadic, JoinTextMixesStringsNumbersAndBools) {
  EXPECT_EQ(tc::join_text("-", std::string_view{"x"}, 2, false), "x-2-false");
  EXPECT_EQ(tc::join_text(", ", std::string{"a"}, std::string{"b"}), "a, b");
}

TEST(Part3Variadic, JoinTextWithEmptyPackIsEmpty) {
  EXPECT_EQ(tc::join_text(","), "");
  EXPECT_EQ(tc::join_text(",", 1), "1");
}

template <typename T>
concept JoinAccepts = requires(T v) { tc::join_text(",", v); };

TEST(Part3Variadic, JoinTextRejectsNonTextTypes) {
  EXPECT_EQ(tc::join_text("|", 1, 2), "1|2");
  EXPECT_FALSE((JoinAccepts<std::vector<int>>));
}

TEST(Part3Variadic, ToTextRendersBoolsAsWords) {
  EXPECT_EQ(tc::to_text(true), "true");
  EXPECT_EQ(tc::to_text(false), "false");
  EXPECT_EQ(tc::to_text(std::string{"plain"}), "plain");
}

TEST(Part3Variadic, KindNameDispatchesOnTypeTraits) {
  EXPECT_EQ(tc::kind_name<bool>(), "boolean");
  EXPECT_EQ(tc::kind_name<int>(), "integer");
  EXPECT_EQ(tc::kind_name<double>(), "floating");
  EXPECT_EQ(tc::kind_name<std::string>(), "text");
  EXPECT_EQ(tc::kind_name<std::vector<int>>(), "other");
}

TEST(Part3Variadic, ParseOrParsesWholeTextOrFallsBack) {
  EXPECT_EQ((tc::parse_or<int>("42", 0)), 42);
  EXPECT_EQ((tc::parse_or<int>("4x", 7)), 7);
  EXPECT_EQ((tc::parse_or<int>("", -1)), -1);
}

TEST(Part3Variadic, ParseOrFloatingPointAndTypeIdentityFallback) {
  EXPECT_DOUBLE_EQ((tc::parse_or<double>("2.5", 0.0)), 2.5);
  // The fallback is a non-deduced context: an int literal converts to double.
  EXPECT_DOUBLE_EQ((tc::parse_or<double>("nope", 2)), 2.0);
}

TEST(Part3Variadic, ParseOrBoolAcceptsExactWords) {
  EXPECT_TRUE((tc::parse_or<bool>("true", false)));
  EXPECT_FALSE((tc::parse_or<bool>("TRUE", false)));
  EXPECT_TRUE((tc::parse_or<bool>("maybe", true)));
}

TEST(Part3Variadic, ParseOrStringAlwaysSucceeds) {
  EXPECT_EQ((tc::parse_or<std::string>("abc", "x")), "abc");
}
