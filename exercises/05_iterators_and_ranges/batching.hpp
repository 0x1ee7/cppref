// Engineer-owned. Part 5 (batched writes, resource-owning read_batches) and Part 6
// (moving average). Eager argument validation lives in non-coroutine wrappers.
#pragma once

#include <concepts>
#include <cstddef>
#include <functional>
#include <istream>
#include <iterator>
#include <memory>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "generator.hpp"

namespace cppref::iterators_and_ranges {

/// Opens a line source. Destroying the returned stream releases the source.
using SourceOpener = std::function<std::unique_ptr<std::istream>()>;

namespace detail {

inline Generator<std::vector<std::string>> read_batches_gen(SourceOpener, std::size_t) {
    throw std::logic_error("not implemented: read_batches_gen");
}

template <std::ranges::input_range R>
Generator<double> moving_average_gen(R, std::size_t) {
    throw std::logic_error("not implemented: moving_average_gen");
}

}  // namespace detail

/// Yields the raw lines of `open_source()` in batches of `size` (the last batch may be shorter).
/// The source is opened on the first pull, not on this call. It is released when the generator
/// finishes, throws, or is destroyed early. `size == 0` throws std::invalid_argument immediately.
inline Generator<std::vector<std::string>> read_batches(SourceOpener open_source, std::size_t size) {
    throw std::logic_error("not implemented: read_batches");
}

/// Splits `rows` into batches of at most `size` values, pulling only what the current batch needs,
/// and calls `write(batch)` per batch with an owned std::vector. Returns the sum of the values
/// `write` returned. `size == 0` throws std::invalid_argument before anything is pulled.
template <std::ranges::input_range R, typename Write>
    requires std::invocable<Write&, std::vector<std::ranges::range_value_t<R>>> &&
             std::convertible_to<
                 std::invoke_result_t<Write&, std::vector<std::ranges::range_value_t<R>>>,
                 std::size_t>
std::size_t write_in_batches(R&&, std::size_t, Write) {
    throw std::logic_error("not implemented: write_in_batches");
}

/// Mean of each full window of `window` consecutive values; partial windows are never emitted.
/// The range is taken by value, so the result owns its input and may outlive the caller's
/// container. `window == 0` throws std::invalid_argument immediately. Lazy.
template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, double>
Generator<double> moving_average(R, std::size_t) {
    throw std::logic_error("not implemented: moving_average");
}

}  // namespace cppref::iterators_and_ranges
