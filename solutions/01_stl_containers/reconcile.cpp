#include "reconcile.hpp"

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <ranges>
#include <stdexcept>
#include <string>

namespace cppref::stl {
namespace {

void require_non_negative(const std::map<std::string, std::int64_t>& quantities, const char* side) {
    for (const auto& [sku, qty] : quantities) {
        if (qty < 0) {
            throw std::invalid_argument(std::string("reconcile: negative quantity in ") + side + " for " + sku);
        }
    }
}

}  // namespace

bool Reconciliation::is_clean() const {
    return missing.empty() && unexpected.empty() && mismatched.empty();
}

Reconciliation reconcile(const std::map<std::string, std::int64_t>& expected,
                         const std::map<std::string, std::int64_t>& counted) {
    require_non_negative(expected, "expected");
    require_non_negative(counted, "counted");

    Reconciliation result;
    // Keys of a std::map are already sorted, so set_difference yields each bucket in one pass.
    std::ranges::set_difference(expected | std::views::keys, counted | std::views::keys,
                                std::inserter(result.missing, result.missing.end()));
    std::ranges::set_difference(counted | std::views::keys, expected | std::views::keys,
                                std::inserter(result.unexpected, result.unexpected.end()));

    for (const auto& [sku, expected_qty] : expected) {
        if (const auto it = counted.find(sku); it != counted.end() && it->second != expected_qty) {
            result.mismatched.emplace(sku, it->second - expected_qty);
        }
    }
    return result;
}

}  // namespace cppref::stl
