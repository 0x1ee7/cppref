#pragma once

#include <cstdint>
#include <vector>

namespace cppref::stl {

/// Money in integer minor units (cents). Integer arithmetic is exact, and there is
/// no float-to-money conversion to get wrong.
using Cents = std::int64_t;

/// From `min_qty` upward, a unit costs `unit_price`.
struct PriceTier {
    std::int64_t min_qty;
    Cents unit_price;

    friend bool operator==(const PriceTier&, const PriceTier&) = default;
};

/// Volume price schedule. The tier with the largest `min_qty <= qty` applies; a
/// boundary quantity belongs to the new tier.
class PriceTiers {
public:
    /// Tiers may arrive in any order and are sorted here. Throws std::invalid_argument
    /// if: there are no tiers; the first tier (after sorting) does not start at 1;
    /// two tiers share a `min_qty`; a `unit_price` is <= 0.
    explicit PriceTiers(std::vector<PriceTier> tiers);

    /// @throws std::invalid_argument if `qty < 1`.
    Cents unit_price(std::int64_t qty) const;

    /// `unit_price(qty) * qty`, exact.
    /// @throws std::invalid_argument if `qty < 1`.
    Cents line_total(std::int64_t qty) const;

private:
    std::vector<PriceTier> tiers_;  // sorted by min_qty, ascending
};

/// Support response-time band. Names are the enumerator spelling.
enum class SlaTier { critical, high, normal, low };

/// Bands on response minutes, upper bounds inclusive: <= 15 critical, <= 60 high,
/// <= 240 normal, otherwise low (including +infinity).
/// @throws std::invalid_argument if negative or NaN.
SlaTier sla_tier(double response_minutes);

}  // namespace cppref::stl
