// Part 4: strong typedefs with a phantom tag.
#include <gtest/gtest.h>

#include <compare>
#include <concepts>
#include <cstdint>
#include <functional>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "strong_id.hpp"

namespace tc = cppref::templates_and_concepts;

TEST(Part4StrongIds, ConstructsAndReadsValue) {
  tc::UserId id{42};
  EXPECT_EQ(id.value(), 42u);
  EXPECT_EQ((tc::StrongId<tc::UserTag, std::uint32_t>{7}).value(), 7u);
}

TEST(Part4StrongIds, ConstructionIsExplicitAndNeverImplicit) {
  EXPECT_EQ(tc::UserId{1}.value(), 1u);
  EXPECT_FALSE((std::is_convertible_v<std::uint64_t, tc::UserId>));
  EXPECT_FALSE((std::is_convertible_v<tc::UserId, std::uint64_t>));
}

TEST(Part4StrongIds, DistinctTagsDoNotMix) {
  EXPECT_TRUE(tc::UserId{1} == tc::UserId{1});
  EXPECT_FALSE((std::equality_comparable_with<tc::UserId, tc::OrderId>));
}

TEST(Part4StrongIds, OrderingFollowsRepresentation) {
  EXPECT_TRUE(tc::UserId{1} < tc::UserId{2});
  EXPECT_EQ(tc::UserId{2} <=> tc::UserId{1}, std::strong_ordering::greater);
  EXPECT_FALSE(tc::UserId{3} == tc::UserId{4});
}

TEST(Part4StrongIds, HashableAsUnorderedMapKey) {
  std::unordered_map<tc::UserId, std::string> names;
  names.emplace(tc::UserId{7}, "ada");
  EXPECT_EQ(names.at(tc::UserId{7}), "ada");
  EXPECT_EQ(std::hash<tc::UserId>{}(tc::UserId{7}),
            std::hash<tc::UserId>{}(tc::UserId{7}));
}

TEST(Part4StrongIds, ZeroCostLayout) {
  EXPECT_EQ(tc::UserId{5}.value(), 5u);
  EXPECT_EQ(sizeof(tc::UserId), sizeof(std::uint64_t));
  EXPECT_EQ(sizeof(tc::StrongId<tc::UserTag, std::uint32_t>), 4u);
  EXPECT_TRUE(std::is_trivially_copyable_v<tc::UserId>);
}
