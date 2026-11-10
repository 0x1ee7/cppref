#include "pricing.hpp"

#include <stdexcept>

namespace cppref::stl {

PriceTiers::PriceTiers(std::vector<PriceTier>) {
    throw std::logic_error("not implemented: PriceTiers::PriceTiers");
}

Cents PriceTiers::unit_price(std::int64_t) const {
    throw std::logic_error("not implemented: PriceTiers::unit_price");
}

Cents PriceTiers::line_total(std::int64_t) const {
    throw std::logic_error("not implemented: PriceTiers::line_total");
}

SlaTier sla_tier(double) {
    throw std::logic_error("not implemented: sla_tier");
}

}  // namespace cppref::stl
