// Part 2: single-allocation string building and fixed-buffer formatting.
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>

namespace cppref::performance {

// Joins fields with ',' and no quoting. Allocation budget: at most one heap allocation, made
// by reserving the exact output size up front. An empty span yields "" without allocating.
// Throws std::invalid_argument if a field contains ',' or '\n'.
std::string join_csv(std::span<const std::string_view> fields);

struct AccessEntry {
    std::string_view method;
    std::string_view path;
    std::uint16_t status;
    std::uint64_t latency_us;
};

// Canonical text: "<method> <path> <status> <latency_us>us", e.g. "GET /v1/items 200 1532us".

// Returns the canonical text. Allocation budget: at most one heap allocation. Compute the size
// with std::formatted_size, reserve it, then std::format_to(std::back_inserter(s), ...).
std::string format_access_line(const AccessEntry& entry);

// Writes the canonical text into `out` with std::format_to_n and returns a view of it. Zero heap
// allocations. If `out` is too small, returns the number of characters the full text needs
// (the contents of `out` are then unspecified).
std::expected<std::string_view, std::size_t> format_access_line_into(std::span<char> out,
                                                                     const AccessEntry& entry);

}  // namespace cppref::performance
