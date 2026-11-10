#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>

namespace cppref::stl {

/// Result of comparing a system-of-record count against a warehouse count.
struct Reconciliation {
    std::set<std::string> missing;                    ///< expected, absent from counted
    std::set<std::string> unexpected;                 ///< counted, absent from expected
    std::map<std::string, std::int64_t> mismatched;   ///< in both, quantities differ: counted - expected

    /// True iff all three collections are empty.
    bool is_clean() const;

    friend bool operator==(const Reconciliation&, const Reconciliation&) = default;
};

/// Compares two sku -> quantity maps. Both maps are sorted by key, so the three
/// buckets can be produced by a merge-style walk (see `std::set_difference`).
/// A counted quantity of 0 against an expected 3 is a mismatch of -3, not a missing SKU.
/// Any negative quantity throws std::invalid_argument.
Reconciliation reconcile(const std::map<std::string, std::int64_t>& expected,
                         const std::map<std::string, std::int64_t>& counted);

}  // namespace cppref::stl
