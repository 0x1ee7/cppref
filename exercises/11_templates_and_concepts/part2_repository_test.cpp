// Part 2: constrained generic repository.
#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "repository.hpp"

namespace tc = cppref::templates_and_concepts;

namespace {

struct User {
  using id_type = int;
  int key;
  std::string name;
  int id() const { return key; }
};

struct Sku {
  using id_type = std::string;
  std::string code;
  int id() const { return 0; }  // Unused placeholder; see Sku::code below.
  std::string sku_id() const { return code; }
};

// Keyed by string id, to show the repository only needs `id_type`.
struct Product {
  using id_type = std::string;
  std::string code;
  int price;
  std::string id() const { return code; }
};

}  // namespace

TEST(Part2Repository, AddThenFindReturnsCopy) {
  tc::Repository<User> repo;
  EXPECT_TRUE(repo.add(User{2, "bo"}));
  auto found = repo.find(2);
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->name, "bo");
  EXPECT_EQ(repo.size(), 1u);
}

TEST(Part2Repository, DuplicateIdIsRejectedAndKeepsOriginal) {
  tc::Repository<User> repo;
  EXPECT_TRUE(repo.add(User{1, "ada"}));
  EXPECT_FALSE(repo.add(User{1, "zed"}));
  EXPECT_EQ(repo.find(1)->name, "ada");
  EXPECT_EQ(repo.size(), 1u);
}

TEST(Part2Repository, FindMissingIdReturnsEmpty) {
  tc::Repository<User> repo;
  EXPECT_TRUE(repo.add(User{5, "eve"}));
  EXPECT_FALSE(repo.find(99).has_value());
  EXPECT_TRUE(repo.empty() == false);
}

TEST(Part2Repository, RemoveReportsWhetherPresent) {
  tc::Repository<User> repo;
  EXPECT_TRUE(repo.add(User{4, "dan"}));
  EXPECT_TRUE(repo.remove(4));
  EXPECT_FALSE(repo.remove(4));
  EXPECT_TRUE(repo.empty());
  EXPECT_EQ(repo.size(), 0u);
}

TEST(Part2Repository, AllIsOrderedByIdNotInsertion) {
  tc::Repository<User> repo;
  EXPECT_TRUE(repo.add(User{3, "c"}));
  EXPECT_TRUE(repo.add(User{1, "a"}));
  EXPECT_TRUE(repo.add(User{2, "b"}));
  std::vector<int> ids;
  for (const auto& u : repo.all()) ids.push_back(u.key);
  EXPECT_EQ(ids, (std::vector<int>{1, 2, 3}));
}

TEST(Part2Repository, WhereFiltersWithPredicateInIdOrder) {
  tc::Repository<User> repo;
  EXPECT_TRUE(repo.add(User{10, "long-name"}));
  EXPECT_TRUE(repo.add(User{20, "x"}));
  EXPECT_TRUE(repo.add(User{30, "medium"}));
  auto long_names = repo.where([](const User& u) { return u.name.size() > 3; });
  ASSERT_EQ(long_names.size(), 2u);
  EXPECT_EQ(long_names[0].key, 10);
  EXPECT_EQ(long_names[1].key, 30);
}

TEST(Part2Repository, StringIdsWorkWithoutChangingTheRepository) {
  tc::Repository<Product> repo;
  EXPECT_TRUE(repo.add(Product{"b-100", 5}));
  EXPECT_TRUE(repo.add(Product{"a-200", 9}));
  auto all = repo.all();
  ASSERT_EQ(all.size(), 2u);
  EXPECT_EQ(all[0].code, "a-200");
  EXPECT_EQ(repo.find(std::string{"b-100"})->price, 5);
}

template <typename T>
concept RepositoryBuildable = requires { typename tc::Repository<T>; };

TEST(Part2Repository, RepositoryOfNonEntityIsNotWellFormed) {
  EXPECT_TRUE(tc::Repository<User>{}.empty());
  EXPECT_FALSE((RepositoryBuildable<int>));
}
