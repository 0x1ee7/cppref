#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cppref::stl {

/// One line of the order pipeline's sales stream.
struct SalesLine {
    std::string sku;
    std::int64_t units;

    friend bool operator==(const SalesLine&, const SalesLine&) = default;
};

/// Aggregated units for one SKU.
struct SkuTotal {
    std::string sku;
    std::int64_t units;

    friend bool operator==(const SkuTotal&, const SkuTotal&) = default;
};

/// Streams `sales` exactly once and returns at most `k` totals, sorted by units
/// descending with ties broken by SKU ascending.
///
/// - Units are aggregated per SKU. Any negative `units` throws std::invalid_argument.
/// - `k == 0` returns an empty vector; `k` larger than the number of distinct SKUs
///   returns all of them.
/// - The input may be a single-pass range (e.g. a view over a socket), and it may
///   contain far more lines than distinct SKUs. Memory should scale with the number
///   of distinct SKUs plus `k`, not with the number of lines.
template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, SalesLine>
std::vector<SkuTotal> top_k_products(R&& sales, std::size_t k) {
    std::unordered_map<std::string, std::int64_t> totals;
    for (auto&& line : sales) {
        // Binds to the reference directly when the range yields one; otherwise to a temporary.
        const SalesLine& sl = line;
        if (sl.units < 0) {
            throw std::invalid_argument("top_k_products: negative units for sku " + sl.sku);
        }
        totals[sl.sku] += sl.units;
    }

    std::vector<SkuTotal> ranked;
    ranked.reserve(totals.size());
    while (!totals.empty()) {
        auto node = totals.extract(totals.begin());
        ranked.push_back(SkuTotal{std::move(node.key()), node.mapped()});
    }

    const auto ranks_before = [](const SkuTotal& a, const SkuTotal& b) {
        return a.units != b.units ? a.units > b.units : a.sku < b.sku;
    };
    const std::size_t keep = std::min(k, ranked.size());
    std::ranges::partial_sort(ranked, ranked.begin() + static_cast<std::ptrdiff_t>(keep), ranks_before);
    ranked.resize(keep);
    return ranked;
}

}  // namespace cppref::stl
