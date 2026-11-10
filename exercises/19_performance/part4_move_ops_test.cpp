// Part 4 tests: move semantics and reallocation policy with instrumented element types.
#include "move_ops.hpp"
#include "perf_support_test.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <numeric>
#include <span>
#include <stdexcept>
#include <vector>

namespace {

using cppref::performance::append_strong;
using cppref::performance::drain_front;
using cppref::performance::emplace_n;
using cppref::performance::testing::AllocScope;
using cppref::performance::testing::container_overhead;
using cppref::performance::testing::ids_of;
using cppref::performance::testing::Ledger;
using cppref::performance::testing::Probe;

// ---- drain_front -------------------------------------------------------------------------------

TEST(Part4Drain, ReturnsFirstMaxInOrder) {
    std::vector<int> pending{1, 2, 3, 4, 5, 6};
    const std::vector<int> out = drain_front(pending, 4);
    EXPECT_EQ(out, (std::vector<int>{1, 2, 3, 4}));
    EXPECT_EQ(pending, (std::vector<int>{5, 6}));
}

TEST(Part4Drain, MaxBeyondSizeReturnsEverything) {
    std::vector<int> pending{1, 2};
    const std::vector<int> out = drain_front(pending, 10);
    EXPECT_EQ(out, (std::vector<int>{1, 2}));
    EXPECT_TRUE(pending.empty());
}

TEST(Part4Drain, ZeroMaxReturnsNothing) {
    std::vector<int> pending{1, 2};
    const std::vector<int> out = drain_front(pending, 0);
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(pending, (std::vector<int>{1, 2}));
}

TEST(Part4Drain, EmptyPendingReturnsNothing) {
    std::vector<int> pending;
    const std::vector<int> out = drain_front(pending, 3);
    EXPECT_TRUE(out.empty());
    EXPECT_TRUE(pending.empty());
}

TEST(Part4Drain, MovesElementsWithoutCopying) {
    std::vector<Probe<true>> pending;
    for (int id = 1; id <= 4; ++id) pending.emplace_back(id);
    Ledger::reset();
    const std::vector<Probe<true>> out = drain_front(pending, 2);
    EXPECT_EQ(ids_of(out), (std::vector<int>{1, 2}));
    EXPECT_EQ(ids_of(pending), (std::vector<int>{3, 4}));
    EXPECT_EQ(Ledger::copies, 0);
}

// ---- emplace_n ---------------------------------------------------------------------------------

TEST(Part4Emplace, AppendsCopiesOfArgument) {
    std::vector<int> dst{1};
    emplace_n(dst, 3, 7);
    EXPECT_EQ(dst, (std::vector<int>{1, 7, 7, 7}));
}

TEST(Part4Emplace, ZeroCountIsNoOp) {
    std::vector<int> dst{1};
    emplace_n(dst, 0, 7);
    EXPECT_EQ(dst, (std::vector<int>{1}));
}

TEST(Part4Emplace, ConstructsEachElementFromArguments) {
    std::vector<Probe<true>> dst;
    emplace_n(dst, 2, 5);
    EXPECT_EQ(ids_of(dst), (std::vector<int>{5, 5}));
}

TEST(Part4Emplace, ReservesOnceForTheWholeBatch) {
    std::vector<int> dst(3, 0);
    AllocScope scope;
    emplace_n(dst, 10, 1);
    const std::size_t allocs = scope.allocations();
    EXPECT_EQ(dst.size(), 13u);
    EXPECT_EQ(allocs, 1u);
}

TEST(Part4Emplace, NoAllocationWhenCapacitySuffices) {
    std::vector<int> dst;
    dst.reserve(20);
    dst.push_back(1);
    dst.push_back(2);
    const int* before = dst.data();
    AllocScope scope;
    emplace_n(dst, 3, 9);
    const std::size_t allocs = scope.allocations();
    EXPECT_EQ(dst, (std::vector<int>{1, 2, 9, 9, 9}));
    EXPECT_EQ(dst.data(), before);
    EXPECT_EQ(allocs, 0u);
}

// ---- append_strong -----------------------------------------------------------------------------

TEST(Part4Append, AppendsItemsInOrder) {
    std::vector<int> dst{1};
    const std::vector<int> items{2, 3};
    append_strong(dst, std::span<const int>(items));
    EXPECT_EQ(dst, (std::vector<int>{1, 2, 3}));
}

TEST(Part4Append, EmptyItemsIsNoOp) {
    std::vector<int> dst{1};
    append_strong(dst, std::span<const int>());
    EXPECT_EQ(dst, (std::vector<int>{1}));
}

TEST(Part4Append, FailedCopyLeavesDestinationUnchanged) {
    std::vector<Probe<true>> dst;
    dst.emplace_back(1);
    std::vector<Probe<true>> items;
    items.emplace_back(2);
    items.emplace_back(3);
    items.emplace_back(4);
    Ledger::reset();
    // The first item copies, the second copy throws.
    Ledger::copies_until_throw = 1;
    EXPECT_THROW(append_strong(dst, std::span<const Probe<true>>(items)), std::runtime_error);
    Ledger::reset();
    EXPECT_EQ(ids_of(dst), (std::vector<int>{1}));
}

TEST(Part4Append, CopiesEachItemExactlyOnce) {
    std::vector<Probe<true>> dst;
    dst.emplace_back(1);
    std::vector<Probe<true>> items;
    items.emplace_back(2);
    items.emplace_back(3);
    items.emplace_back(4);
    Ledger::reset();
    append_strong(dst, std::span<const Probe<true>>(items));
    EXPECT_EQ(ids_of(dst), (std::vector<int>{1, 2, 3, 4}));
    EXPECT_EQ(Ledger::copies, 3);
}

// ---- Allocation budgets ------------------------------------------------------------------------

TEST(Part4Alloc, NonEmptyDrainAllocatesOnce) {
    std::vector<int> pending(100);
    std::iota(pending.begin(), pending.end(), 0);
    const std::size_t overhead = container_overhead<std::vector<int>>();
    AllocScope scope;
    const std::vector<int> out = drain_front(pending, 10);
    const std::size_t allocs = scope.allocations() - overhead;
    EXPECT_EQ(out.size(), 10u);
    EXPECT_EQ(pending.size(), 90u);
    EXPECT_EQ(allocs, 1u);
}

TEST(Part4Alloc, EmptyDrainAllocatesNothing) {
    std::vector<int> pending;
    const std::size_t overhead = container_overhead<std::vector<int>>();
    AllocScope scope;
    const std::vector<int> out = drain_front(pending, 5);
    const std::size_t allocs = scope.allocations() - overhead;
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(allocs, 0u);
}

TEST(Part4Alloc, ZeroMaxDrainAllocatesNothing) {
    std::vector<int> pending{1, 2, 3};
    const std::size_t overhead = container_overhead<std::vector<int>>();
    AllocScope scope;
    const std::vector<int> out = drain_front(pending, 0);
    const std::size_t allocs = scope.allocations() - overhead;
    EXPECT_TRUE(out.empty());
    EXPECT_EQ(allocs, 0u);
}

}  // namespace
