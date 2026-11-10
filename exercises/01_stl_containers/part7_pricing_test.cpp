#include "pricing.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

namespace {

using cppref::stl::Cents;
using cppref::stl::PriceTier;
using cppref::stl::PriceTiers;
using cppref::stl::SlaTier;
using cppref::stl::sla_tier;

PriceTiers standard_tiers() {
    return PriceTiers(std::vector<PriceTier>{{1, 1000}, {10, 900}, {50, 800}});
}

}  // namespace

TEST(Part7Pricing, UnitPriceChangesExactlyAtTierBoundaries) {
    const PriceTiers tiers = standard_tiers();

    EXPECT_EQ(tiers.unit_price(1), 1000);
    EXPECT_EQ(tiers.unit_price(9), 1000);
    EXPECT_EQ(tiers.unit_price(10), 900);  // boundary belongs to the new tier
    EXPECT_EQ(tiers.unit_price(49), 900);
    EXPECT_EQ(tiers.unit_price(50), 800);
    EXPECT_EQ(tiers.unit_price(1'000'000), 800);
}

TEST(Part7Pricing, TiersMayBeGivenUnsorted) {
    const PriceTiers tiers(std::vector<PriceTier>{{50, 800}, {1, 1000}, {10, 900}});

    EXPECT_EQ(tiers.unit_price(10), 900);
    EXPECT_EQ(tiers.unit_price(49), 900);
}

TEST(Part7Pricing, LineTotalIsUnitPriceTimesQuantity) {
    const PriceTiers tiers = standard_tiers();

    EXPECT_EQ(tiers.line_total(9), 9000);
    EXPECT_EQ(tiers.line_total(10), 9000);  // 10 * 900, not 10 * 1000
    EXPECT_EQ(tiers.line_total(50), 40000);
}

TEST(Part7Pricing, RejectsEmptyTiers) {
    EXPECT_THROW(PriceTiers(std::vector<PriceTier>{}), std::invalid_argument);
}

TEST(Part7Pricing, FirstTierMustStartAtOne) {
    EXPECT_THROW(PriceTiers(std::vector<PriceTier>{{2, 1000}}), std::invalid_argument);
    EXPECT_THROW(PriceTiers(std::vector<PriceTier>{{0, 1000}, {5, 900}}), std::invalid_argument);
}

TEST(Part7Pricing, RejectsDuplicateMinQtyAndNonPositivePrice) {
    EXPECT_THROW(PriceTiers(std::vector<PriceTier>{{1, 1000}, {1, 900}}), std::invalid_argument);
    EXPECT_THROW(PriceTiers(std::vector<PriceTier>{{1, 0}}), std::invalid_argument);
    EXPECT_THROW(PriceTiers(std::vector<PriceTier>{{1, -5}}), std::invalid_argument);
}

TEST(Part7Pricing, RejectsNonPositiveQuantity) {
    const PriceTiers tiers = standard_tiers();

    EXPECT_THROW(tiers.unit_price(0), std::invalid_argument);
    EXPECT_THROW(tiers.line_total(-1), std::invalid_argument);
}

TEST(Part7Pricing, SlaBandBoundariesAreUpperInclusive) {
    EXPECT_EQ(sla_tier(0.0), SlaTier::critical);
    EXPECT_EQ(sla_tier(15.0), SlaTier::critical);
    EXPECT_EQ(sla_tier(15.0001), SlaTier::high);
    EXPECT_EQ(sla_tier(60.0), SlaTier::high);
    EXPECT_EQ(sla_tier(61.0), SlaTier::normal);
    EXPECT_EQ(sla_tier(240.0), SlaTier::normal);
    EXPECT_EQ(sla_tier(240.5), SlaTier::low);
}

TEST(Part7Pricing, SlaInfinityIsLowAndBadInputsAreRejected) {
    EXPECT_EQ(sla_tier(std::numeric_limits<double>::infinity()), SlaTier::low);
    EXPECT_THROW(sla_tier(-1.0), std::invalid_argument);
    EXPECT_THROW(sla_tier(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
}
