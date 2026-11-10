#include "pricing.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <iterator>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cppref::stl {

PriceTiers::PriceTiers(std::vector<PriceTier> tiers) : tiers_{std::move(tiers)} {
    if (tiers_.empty()) {
        throw std::invalid_argument("PriceTiers: at least one tier is required");
    }
    std::ranges::sort(tiers_, {}, &PriceTier::min_qty);
    if (tiers_.front().min_qty != 1) {
        throw std::invalid_argument("PriceTiers: first tier must start at quantity 1");
    }
    for (std::size_t i = 0; i < tiers_.size(); ++i) {
        if (tiers_[i].unit_price <= 0) {
            throw std::invalid_argument("PriceTiers: unit price must be positive");
        }
        if (i > 0 && tiers_[i].min_qty == tiers_[i - 1].min_qty) {
            throw std::invalid_argument("PriceTiers: duplicate min_qty");
        }
    }
}

Cents PriceTiers::unit_price(std::int64_t qty) const {
    if (qty < 1) {
        throw std::invalid_argument("PriceTiers: quantity must be >= 1");
    }
    // First tier starting strictly above qty; its predecessor is the applicable tier.
    const auto next = std::ranges::upper_bound(tiers_, qty, {}, &PriceTier::min_qty);
    return std::prev(next)->unit_price;
}

Cents PriceTiers::line_total(std::int64_t qty) const {
    return unit_price(qty) * qty;
}

SlaTier sla_tier(double response_minutes) {
    if (std::isnan(response_minutes) || response_minutes < 0.0) {
        throw std::invalid_argument("sla_tier: response minutes must be a non-negative number");
    }
    static constexpr std::array<double, 3> kUpperBounds{15.0, 60.0, 240.0};
    static constexpr std::array<SlaTier, 4> kBands{SlaTier::critical, SlaTier::high, SlaTier::normal, SlaTier::low};

    // lower_bound finds the first bound >= x, which is the inclusive upper edge of x's band.
    const auto idx = std::ranges::lower_bound(kUpperBounds, response_minutes) - kUpperBounds.begin();
    return kBands[static_cast<std::size_t>(idx)];
}

}  // namespace cppref::stl
