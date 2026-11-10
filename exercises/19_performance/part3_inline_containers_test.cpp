// Part 3 tests: small-buffer containers that avoid the heap under their inline capacity.
#include "inline_containers.hpp"
#include "perf_support_test.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

using cppref::performance::InlineVector;
using cppref::performance::SmallString;
using cppref::performance::testing::AllocScope;
using cppref::performance::testing::ids_of;
using cppref::performance::testing::Ledger;
using cppref::performance::testing::Probe;

// ---- InlineVector: behaviour -------------------------------------------------------------------

TEST(Part3Vector, StaysInlineUpToCapacity) {
    InlineVector<int, 4> v;
    for (int i = 1; i <= 4; ++i) v.push_back(i);
    EXPECT_TRUE(v.is_inline());
    EXPECT_EQ(v.size(), 4u);
    EXPECT_EQ(v.capacity(), 4u);
    EXPECT_EQ(v[3], 4);
}

TEST(Part3Vector, SpillKeepsElementsInOrder) {
    InlineVector<int, 2> v;
    for (int i = 1; i <= 3; ++i) v.push_back(i);
    EXPECT_FALSE(v.is_inline());
    EXPECT_EQ(std::vector<int>(v.begin(), v.end()), (std::vector<int>{1, 2, 3}));
}

TEST(Part3Vector, CapacityDoublesOnSpill) {
    InlineVector<int, 2> v;
    for (int i = 1; i <= 3; ++i) v.push_back(i);
    EXPECT_EQ(v.capacity(), 4u);
    v.push_back(4);
    EXPECT_EQ(v.capacity(), 4u);
    v.push_back(5);
    EXPECT_EQ(v.capacity(), 8u);
}

TEST(Part3Vector, CopyIsDeep) {
    InlineVector<std::string, 2> a;
    a.emplace_back("x");
    a.emplace_back("y");
    a.emplace_back("z");
    InlineVector<std::string, 2> b(a);
    b[0] = "changed";
    EXPECT_EQ(a[0], "x");
    EXPECT_EQ(b.size(), 3u);
    EXPECT_EQ(b[0], "changed");
}

TEST(Part3Vector, CopyAssignmentReplacesContents) {
    InlineVector<int, 2> a;
    for (int i = 1; i <= 3; ++i) a.push_back(i);
    InlineVector<int, 2> b;
    b.push_back(9);
    b = a;
    EXPECT_EQ(std::vector<int>(b.begin(), b.end()), (std::vector<int>{1, 2, 3}));
}

TEST(Part3Vector, MovingInlineVectorMovesElements) {
    Ledger::reset();
    InlineVector<Probe<true>, 4> v;
    v.emplace_back(1);
    v.emplace_back(2);
    InlineVector<Probe<true>, 4> w(std::move(v));
    EXPECT_EQ(ids_of(w), (std::vector<int>{1, 2}));
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(Ledger::copies, 0);
}

TEST(Part3Vector, MovingSpilledVectorStealsBlock) {
    InlineVector<int, 2> v;
    for (int i = 1; i <= 5; ++i) v.push_back(i);
    auto* block = v.data();
    InlineVector<int, 2> w(std::move(v));
    EXPECT_EQ(w.data(), block);
    EXPECT_EQ(w.size(), 5u);
    EXPECT_TRUE(v.empty());
}

TEST(Part3Vector, MoveAssignmentEmptiesSource) {
    InlineVector<int, 2> v;
    for (int i = 1; i <= 3; ++i) v.push_back(i);
    InlineVector<int, 2> w;
    w = std::move(v);
    EXPECT_EQ(w.size(), 3u);
    EXPECT_TRUE(v.empty());
}

TEST(Part3Vector, PopBackRemovesLastElement) {
    InlineVector<int, 2> v;
    for (int i = 1; i <= 3; ++i) v.push_back(i);
    v.pop_back();
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(v[1], 2);
}

TEST(Part3Vector, PopBackDestroysTheElement) {
    Ledger::reset();
    InlineVector<Probe<true>, 4> v;
    v.emplace_back(1);
    v.emplace_back(2);
    v.pop_back();
    EXPECT_EQ(Ledger::live, 1);
    EXPECT_EQ(v.size(), 1u);
}

TEST(Part3Vector, ClearDestroysElementsAndKeepsCapacity) {
    Ledger::reset();
    InlineVector<Probe<true>, 2> v;
    for (int i = 0; i < 3; ++i) v.emplace_back(i);
    EXPECT_EQ(Ledger::live, 3);
    const std::size_t cap = v.capacity();
    v.clear();
    EXPECT_EQ(Ledger::live, 0);
    EXPECT_TRUE(v.empty());
    EXPECT_EQ(v.capacity(), cap);
}

TEST(Part3Vector, EmplaceBackFromOwnElementIsSafe) {
    InlineVector<int, 2> v;
    v.push_back(1);
    v.push_back(2);
    v.emplace_back(v[0]);  // forces a spill; the argument refers to a relocated element
    EXPECT_EQ(v.size(), 3u);
    EXPECT_EQ(v[2], 1);
}

TEST(Part3Vector, FailedSpillLeavesVectorUnchanged) {
    Ledger::reset();
    InlineVector<Probe<false>, 2> v;
    v.push_back(Probe<false>(1));
    v.push_back(Probe<false>(2));
    // Relocation must copy (the move is not noexcept); the first copy fails.
    Ledger::copies_until_throw = 0;
    EXPECT_THROW(v.push_back(Probe<false>(3)), std::runtime_error);
    Ledger::reset();
    EXPECT_EQ(v.size(), 2u);
    EXPECT_EQ(ids_of(v), (std::vector<int>{1, 2}));
}

// ---- SmallString: behaviour --------------------------------------------------------------------

TEST(Part3String, AssignAndView) {
    SmallString<8> s;
    s.assign("hi");
    EXPECT_EQ(s.view(), "hi");
    EXPECT_EQ(s.size(), 2u);
    EXPECT_TRUE(s.is_inline());
    EXPECT_EQ(s.capacity(), 8u);
}

TEST(Part3String, AppendAndPushBack) {
    SmallString<8> s;
    s.assign("ab");
    s.append("cd");
    s.push_back('e');
    EXPECT_EQ(s.view(), "abcde");
}

TEST(Part3String, SpillsBeyondInlineCapacity) {
    SmallString<4> s;
    s.assign("abcdef");
    EXPECT_FALSE(s.is_inline());
    EXPECT_EQ(s.view(), "abcdef");
    EXPECT_EQ(s.size(), 6u);
}

TEST(Part3String, CopyIsDeep) {
    SmallString<4> a("hello, world");
    SmallString<4> b(a);
    b.push_back('!');
    EXPECT_EQ(a.view(), "hello, world");
    EXPECT_EQ(b.view(), "hello, world!");
}

TEST(Part3String, MoveStealsSpilledBlock) {
    SmallString<4> s("abcdefgh");
    const char* block = s.data();
    SmallString<4> t(std::move(s));
    EXPECT_EQ(t.data(), block);
    EXPECT_EQ(t.view(), "abcdefgh");
    EXPECT_TRUE(s.empty());
}

TEST(Part3String, ClearKeepsCapacity) {
    SmallString<4> s("abcdefgh");
    const std::size_t cap = s.capacity();
    s.clear();
    EXPECT_TRUE(s.empty());
    EXPECT_EQ(s.capacity(), cap);
}

// ---- Allocation budgets ------------------------------------------------------------------------

TEST(Part3Alloc, FillingInlineStorageNeverAllocates) {
    InlineVector<int, 8> v;
    AllocScope scope;
    for (int i = 0; i < 8; ++i) v.push_back(i);
    const std::size_t allocs = scope.allocations();
    EXPECT_EQ(v.size(), 8u);
    EXPECT_EQ(allocs, 0u);
}

TEST(Part3Alloc, FirstSpillAllocatesOnce) {
    InlineVector<int, 8> v;
    for (int i = 0; i < 8; ++i) v.push_back(i);
    AllocScope scope;
    v.push_back(8);
    const std::size_t allocs = scope.allocations();
    EXPECT_EQ(v.size(), 9u);
    EXPECT_EQ(allocs, 1u);
}

TEST(Part3Alloc, MovingSpilledVectorAllocatesNothing) {
    InlineVector<int, 2> v;
    for (int i = 1; i <= 5; ++i) v.push_back(i);
    AllocScope scope;
    InlineVector<int, 2> w(std::move(v));
    const std::size_t allocs = scope.allocations();
    EXPECT_EQ(w.size(), 5u);
    EXPECT_EQ(allocs, 0u);
}

TEST(Part3Alloc, SmallStringInlineAssignAllocatesNothing) {
    SmallString<16> s;
    AllocScope scope;
    s.assign("0123456789abcdef");
    const std::size_t allocs = scope.allocations();
    EXPECT_EQ(s.view(), "0123456789abcdef");
    EXPECT_EQ(allocs, 0u);
}

TEST(Part3Alloc, SmallStringSpillAllocatesOnce) {
    const std::string text(17, 'x');
    SmallString<16> s;
    AllocScope scope;
    s.assign(text);
    const std::size_t allocs = scope.allocations();
    EXPECT_EQ(s.size(), 17u);
    EXPECT_EQ(allocs, 1u);
}

}  // namespace
