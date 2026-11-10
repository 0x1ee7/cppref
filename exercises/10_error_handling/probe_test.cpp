#include "probe.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace eh = cppref::error_handling;

namespace {

// Fills a vector by emplace_back and returns the tallies it produced.
template <bool NothrowMove>
eh::ProbeCounts grow(std::size_t n, std::shared_ptr<eh::ProbeCounts> counts) {
    std::vector<eh::Probe<NothrowMove>> v;
    for (std::size_t i = 0; i < n; ++i) {
        v.emplace_back(static_cast<int>(i), counts);
    }
    return *counts;
}

}  // namespace

// --- Part 4: noexcept move, observed through std::vector ---------------------

TEST(Part4Noexcept, NothrowMoveProbeIsNothrowMoveConstructible) {
    auto counts = std::make_shared<eh::ProbeCounts>();
    eh::Probe<true> p(1, counts);
    EXPECT_TRUE(std::is_nothrow_move_constructible_v<eh::Probe<true>>);
    EXPECT_EQ(p.value(), 1);
}

TEST(Part4Noexcept, ThrowingMoveProbeIsNotNothrowMoveConstructible) {
    auto counts = std::make_shared<eh::ProbeCounts>();
    eh::Probe<false> p(1, counts);
    EXPECT_FALSE(std::is_nothrow_move_constructible_v<eh::Probe<false>>);
    EXPECT_EQ(p.value(), 1);
}

TEST(Part4Noexcept, VectorGrowthMovesNothrowProbes) {
    auto counts = std::make_shared<eh::ProbeCounts>();
    const eh::ProbeCounts seen = grow<true>(5, counts);
    EXPECT_EQ(seen.copies, 0u);
    EXPECT_GT(seen.moves, 0u);
}

TEST(Part4Noexcept, VectorGrowthCopiesThrowingMoveProbes) {
    auto counts = std::make_shared<eh::ProbeCounts>();
    const eh::ProbeCounts seen = grow<false>(5, counts);
    EXPECT_GT(seen.copies, 0u);
}

TEST(Part4Noexcept, CopyCountsAndPreservesValue) {
    auto counts = std::make_shared<eh::ProbeCounts>();
    const eh::Probe<true> original(42, counts);
    const eh::Probe<true> copy(original);
    EXPECT_EQ(copy.value(), 42);
    EXPECT_EQ(counts->copies, 1u);
}
