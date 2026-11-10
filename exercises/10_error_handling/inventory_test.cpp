#include "inventory.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

namespace eh = cppref::error_handling;

namespace {

// Number of copies that may still succeed. -1 = never throw, 0 = next copy throws.
int g_copies_left = -1;

// Copyable entry whose copy constructor can be made to throw. Moves are
// noexcept, so vector growth never copies and the counter only sees real copies.
struct Flaky {
    int id = 0;

    explicit Flaky(int i) : id(i) {}
    Flaky(const Flaky& other) : id(other.id) {
        if (g_copies_left == 0) {
            throw std::runtime_error("copy failed");
        }
        if (g_copies_left > 0) {
            --g_copies_left;
        }
    }
    Flaky(Flaky&&) noexcept = default;
    Flaky& operator=(const Flaky&) = default;
    Flaky& operator=(Flaky&&) noexcept = default;
    ~Flaky() = default;

    friend bool operator==(const Flaky&, const Flaky&) = default;
};

std::vector<int> ids_of(std::span<const Flaky> entries) {
    std::vector<int> out;
    for (const auto& e : entries) {
        out.push_back(e.id);
    }
    return out;
}

std::vector<Flaky> make_batch(std::initializer_list<int> ids) {
    std::vector<Flaky> out;
    for (int id : ids) {
        out.emplace_back(id);
    }
    return out;
}

}  // namespace

// --- Part 3: exception-safety guarantees -------------------------------------

class Part3Guarantees : public ::testing::Test {
protected:
    void SetUp() override { g_copies_left = -1; }
    void TearDown() override { g_copies_left = -1; }
};

TEST_F(Part3Guarantees, ApplyAppendsBatchInOrder) {
    eh::Inventory<Flaky> inv;
    const auto first = make_batch({1, 2});
    const auto second = make_batch({3, 4, 5});
    inv.apply(first);
    inv.apply(second);
    EXPECT_EQ(ids_of(inv.entries()), (std::vector<int>{1, 2, 3, 4, 5}));
}

TEST_F(Part3Guarantees, ApplyCanViewTheLedgersOwnEntries) {
    eh::Inventory<Flaky> inv;
    inv.apply(make_batch({1, 2}));
    inv.apply(inv.entries());
    EXPECT_EQ(ids_of(inv.entries()), (std::vector<int>{1, 2, 1, 2}));
}

TEST_F(Part3Guarantees, ApplyStrongGuaranteeLeavesLedgerUntouchedOnFailure) {
    eh::Inventory<Flaky> inv;
    inv.apply(make_batch({1, 2}));
    const auto batch = make_batch({7, 8, 9});

    g_copies_left = 1;  // the second batch copy throws
    EXPECT_THROW(inv.apply(batch), std::runtime_error);
    EXPECT_EQ(ids_of(inv.entries()), (std::vector<int>{1, 2}));
}

TEST_F(Part3Guarantees, ApplyInPlaceKeepsPartialWorkAndStaysUsable) {
    eh::Inventory<Flaky> inv;
    inv.apply(make_batch({1, 2}));
    const auto batch = make_batch({7, 8, 9});

    g_copies_left = 1;  // 7 is applied, then the copy of 8 throws
    EXPECT_THROW(inv.apply_in_place(batch), std::runtime_error);
    g_copies_left = -1;

    EXPECT_EQ(ids_of(inv.entries()), (std::vector<int>{1, 2, 7}));
    inv.apply(make_batch({10}));
    EXPECT_EQ(ids_of(inv.entries()), (std::vector<int>{1, 2, 7, 10}));
}

TEST_F(Part3Guarantees, ClearEmptiesTheLedger) {
    eh::Inventory<Flaky> inv;
    inv.apply(make_batch({1, 2}));
    inv.clear();
    EXPECT_EQ(inv.size(), 0u);
    EXPECT_TRUE(inv.entries().empty());
}

// --- Part 3 (cont.): the strong guarantee at every failure point -------------

class Part3StrongApply : public ::testing::TestWithParam<int> {
protected:
    void SetUp() override { g_copies_left = -1; }
    void TearDown() override { g_copies_left = -1; }
};

TEST_P(Part3StrongApply, FailureAtAnyCopyLeavesLedgerExactlyAsBefore) {
    eh::Inventory<Flaky> inv;
    inv.apply(make_batch({1, 2}));
    const auto batch = make_batch({7, 8, 9});

    g_copies_left = GetParam();  // the batch alone needs 3 copies, so this always throws
    EXPECT_THROW(inv.apply(batch), std::runtime_error);
    g_copies_left = -1;

    EXPECT_EQ(inv.size(), 2u);
    EXPECT_EQ(ids_of(inv.entries()), (std::vector<int>{1, 2}));
}

INSTANTIATE_TEST_SUITE_P(FailurePoints, Part3StrongApply, ::testing::Values(0, 1, 2));
