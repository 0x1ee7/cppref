#pragma once

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <ranges>
#include <stdexcept>
#include <string>
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
std::vector<SkuTotal> top_k_products(R&&, std::size_t) {
    throw std::logic_error("not implemented: top_k_products");
}

}  // namespace cppref::stl
