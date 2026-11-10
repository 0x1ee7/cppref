#include "persistent_list.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace fn = cppref::functional;

namespace {

using IntList = fn::PersistentList<int>;

}  // namespace

TEST(Part2PersistentList, FromVectorKeepsOrder) {
    auto list = IntList::from_vector({1, 2, 3});
    EXPECT_EQ(list.size(), 3u);
    EXPECT_EQ(list.front(), 1);
    EXPECT_EQ(list.to_vector(), (std::vector<int>{1, 2, 3}));
}

TEST(Part2PersistentList, DefaultListIsEmptyAndRejectsFront) {
    IntList empty;
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.size(), 0u);
    EXPECT_TRUE(empty.to_vector().empty());
    EXPECT_THROW((void)empty.front(), std::out_of_range);
    EXPECT_THROW((void)empty.pop_front(), std::out_of_range);
}

TEST(Part2PersistentList, ConsLeavesOriginalUntouched) {
    auto a = IntList::from_vector({1, 2});
    auto b = a.cons(0);
    EXPECT_EQ(a.to_vector(), (std::vector<int>{1, 2}));
    EXPECT_EQ(b.to_vector(), (std::vector<int>{0, 1, 2}));
}

TEST(Part2PersistentList, PopFrontReturnsTailWithoutMutating) {
    auto a = IntList::from_vector({1, 2});
    auto tail = a.pop_front();
    EXPECT_EQ(tail.to_vector(), (std::vector<int>{2}));
    EXPECT_EQ(a.size(), 2u);
}

TEST(Part2PersistentList, GetIsBoundsChecked) {
    auto a = IntList::from_vector({10, 20});
    EXPECT_EQ(a.get(1), 20);
    EXPECT_FALSE(a.get(2).has_value());
    EXPECT_FALSE(a.get(100).has_value());
}

TEST(Part2PersistentList, SetCopiesOnlyTheChangedVersion) {
    auto a = IntList::from_vector({1, 2, 3});
    auto b = a.set(1, 20);
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(b->to_vector(), (std::vector<int>{1, 20, 3}));
    EXPECT_EQ(a.to_vector(), (std::vector<int>{1, 2, 3}));
    EXPECT_FALSE(a.set(3, 0).has_value());
}

TEST(Part2PersistentList, SetSharesTheUnchangedSuffix) {
    auto a = IntList::from_vector({1, 2, 3, 4});
    auto b = a.set(1, 20);
    ASSERT_TRUE(b.has_value());
    EXPECT_FALSE(b->shares_node_at(a, 0));  // prefix was rebuilt
    EXPECT_FALSE(b->shares_node_at(a, 1));  // replaced node
    EXPECT_TRUE(b->shares_node_at(a, 2));   // suffix is shared, not copied
    EXPECT_TRUE(b->shares_node_at(a, 3));
}

TEST(Part2PersistentList, ConsSharesTheWholeOriginal) {
    auto a = IntList::from_vector({1, 2});
    auto b = a.cons(0);
    EXPECT_TRUE(b.pop_front().shares_node_at(a, 0));
}

TEST(Part2PersistentList, MapChangesElementTypeWithoutTouchingSource) {
    auto a = IntList::from_vector({1, 2, 3});
    auto strs = a.map([](int x) { return std::to_string(x * 10); });
    EXPECT_EQ(strs.to_vector(), (std::vector<std::string>{"10", "20", "30"}));
    EXPECT_EQ(a.front(), 1);
}

TEST(Part2PersistentList, EqualityComparesElements) {
    EXPECT_TRUE(IntList::from_vector({1, 2}) == IntList::from_vector({1, 2}));
    EXPECT_FALSE(IntList::from_vector({1, 2}) == IntList::from_vector({1, 3}));
    EXPECT_FALSE(IntList{} == IntList::from_vector({1}));
}

TEST(Part2PersistentList, VeryLongListDestructsWithoutStackOverflow) {
    // A naive recursive node destructor would need one stack frame per node.
    IntList list;
    for (int i = 0; i < 200000; ++i) {
        list = list.cons(i);
    }
    EXPECT_EQ(list.size(), 200000u);
    EXPECT_EQ(list.front(), 199999);
}
