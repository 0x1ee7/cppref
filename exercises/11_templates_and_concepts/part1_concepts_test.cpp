// Part 1: concepts, requires-clauses and subsumption.
#include <gtest/gtest.h>

#include <ranges>
#include <string>
#include <vector>

#include "concepts.hpp"

namespace tc = cppref::templates_and_concepts;

namespace {

struct Customer {
  using id_type = int;
  int key;
  int id() const { return key; }
};

struct Account {
  using id_type = int;
  int key;
  std::string owner;
  int id() const { return key; }
  std::string name() const { return owner; }
};

struct Plain {
  int x;
};

// Has an id, but the id type has no ordering: fails Entity.
struct NoOrderId {
  bool operator==(const NoOrderId&) const = default;
};
struct Unorderable {
  using id_type = NoOrderId;
  NoOrderId id() const { return {}; }
};

}  // namespace

TEST(Part1Concepts, EntityAcceptsTypesWithOrderedIdAccessor) {
  EXPECT_EQ(tc::describe(Customer{7}), "entity:7");
  EXPECT_TRUE((tc::Entity<Customer>));
  EXPECT_TRUE((tc::Entity<Account>));
}

TEST(Part1Concepts, EntityRejectsPlainTypesAndUnorderableIds) {
  EXPECT_EQ(tc::describe(Customer{1}), "entity:1");
  EXPECT_FALSE((tc::Entity<Plain>));
  EXPECT_FALSE((tc::Entity<Unorderable>));
  EXPECT_FALSE((tc::Entity<int>));
}

TEST(Part1Concepts, NamedIsStructuralNotNominal) {
  EXPECT_EQ(tc::describe(Account{3, "ada"}), "entity:3:ada");
  EXPECT_TRUE((tc::Named<Account>));
  EXPECT_FALSE((tc::Named<Customer>));
}

TEST(Part1Concepts, DescribeFallsBackToUnconstrainedOverload) {
  EXPECT_EQ(tc::describe(Plain{5}), "value");
  EXPECT_EQ(tc::describe(42), "value");
}

TEST(Part1Concepts, DescribeMostConstrainedOverloadWins) {
  // Account satisfies Entity && Named, so the most constrained overload wins
  // over the Entity-only one that it also satisfies.
  EXPECT_EQ(tc::describe(Account{9, "grace"}), "entity:9:grace");
  EXPECT_EQ(tc::describe(Customer{9}), "entity:9");
}

TEST(Part1Concepts, TotalOfAcceptsLvalueRangesAndViews) {
  std::vector<int> values{1, 2, 3};
  EXPECT_EQ(tc::total_of(values), 6);
  EXPECT_EQ(tc::total_of(std::views::iota(1, 4)), 6);
  EXPECT_EQ(tc::total_of(std::vector<int>{10, 20}), 30);
}

template <typename R>
concept TotalOfAccepts = requires(R&& r) { tc::total_of(std::forward<R>(r)); };

TEST(Part1Concepts, TotalOfRejectsElementsNotConvertibleToLongLong) {
  EXPECT_EQ(tc::total_of(std::vector<int>{4}), 4);
  EXPECT_FALSE((TotalOfAccepts<std::vector<std::string>&>));
}
