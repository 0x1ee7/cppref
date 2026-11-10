#include "reconcile.hpp"

#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace {

using cppref::stl::Reconciliation;
using cppref::stl::reconcile;

using Counts = std::map<std::string, std::int64_t>;

}  // namespace

TEST(Part6Reconcile, IdenticalOrEmptyInputsAreClean) {
    const Counts counts = {{"A", 3}, {"B", 1}};

    EXPECT_TRUE(reconcile(counts, counts).is_clean());
    EXPECT_TRUE(reconcile({}, {}).is_clean());
}

TEST(Part6Reconcile, ReportsMissingAndUnexpectedSkus) {
    const Counts expected = {{"A", 3}, {"B", 2}, {"C", 1}};
    const Counts counted = {{"B", 2}, {"D", 4}};

    const Reconciliation result = reconcile(expected, counted);

    EXPECT_EQ(result.missing, (std::set<std::string>{"A", "C"}));
    EXPECT_EQ(result.unexpected, (std::set<std::string>{"D"}));
    EXPECT_TRUE(result.mismatched.empty());
    EXPECT_FALSE(result.is_clean());
}

TEST(Part6Reconcile, MismatchIsCountedMinusExpectedInSkuOrder) {
    const Counts expected = {{"B", 5}, {"A", 3}};
    const Counts counted = {{"A", 1}, {"B", 7}};

    const Reconciliation result = reconcile(expected, counted);

    EXPECT_TRUE(result.missing.empty());
    EXPECT_TRUE(result.unexpected.empty());
    EXPECT_EQ(result.mismatched, (std::map<std::string, std::int64_t>{{"A", -2}, {"B", 2}}));
    EXPECT_FALSE(result.is_clean());
}

TEST(Part6Reconcile, ZeroCountedIsAMismatchNotAMissingSku) {
    const Reconciliation result = reconcile({{"A", 3}}, {{"A", 0}});

    EXPECT_TRUE(result.missing.empty());
    EXPECT_EQ(result.mismatched, (std::map<std::string, std::int64_t>{{"A", -3}}));
}

TEST(Part6Reconcile, NegativeQuantitiesAreRejectedOnEitherSide) {
    EXPECT_THROW(reconcile({{"A", -1}}, {{"A", 1}}), std::invalid_argument);
    EXPECT_THROW(reconcile({{"A", 1}}, {{"A", -1}}), std::invalid_argument);
}
