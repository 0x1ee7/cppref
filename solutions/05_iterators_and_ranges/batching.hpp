// Part 5 (batched writes, resource-owning read_batches) and Part 6 (moving average).
// Eager argument validation lives in non-coroutine wrappers.
#pragma once

#include <concepts>
#include <cstddef>
#include <deque>
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

inline Generator<std::vector<std::string>> read_batches_gen(SourceOpener open_source,
                                                            std::size_t size) {
    // The source lives in this frame: it is released when the body finishes, throws, or the
    // generator is destroyed early.
    auto source = open_source();
    if (!source) {
        throw std::runtime_error("read_batches: opener returned no stream");
    }
    std::vector<std::string> batch;
    batch.reserve(size);
    std::string line;
    while (std::getline(*source, line)) {
        batch.push_back(std::move(line));
        line.clear();
        if (batch.size() == size) {
            co_yield std::move(batch);
            batch.clear();
            batch.reserve(size);
        }
    }
    if (!batch.empty()) {
        co_yield std::move(batch);
    }
}

template <std::ranges::input_range R>
Generator<double> moving_average_gen(R values, std::size_t window) {
    // `values` is owned by this frame; the window keeps a running sum.
    std::deque<double> buffer;
    double sum = 0.0;
    for (auto&& v : values) {
        const auto x = static_cast<double>(v);
        buffer.push_back(x);
        sum += x;
        if (buffer.size() > window) {
            sum -= buffer.front();
            buffer.pop_front();
        }
        if (buffer.size() == window) {
            co_yield sum / static_cast<double>(window);
        }
    }
}

}  // namespace detail

/// Yields the raw lines of `open_source()` in batches of `size`. The source is opened on the
/// first pull. `size == 0` throws std::invalid_argument immediately.
inline Generator<std::vector<std::string>> read_batches(SourceOpener open_source, std::size_t size) {
    if (size == 0) {
        throw std::invalid_argument("read_batches: size must be > 0");
    }
    return detail::read_batches_gen(std::move(open_source), size);
}

/// Splits `rows` into batches of at most `size` values, pulling only what the current batch needs,
/// and calls `write(batch)` per batch. Returns the sum of the values `write` returned.
template <std::ranges::input_range R, typename Write>
    requires std::invocable<Write&, std::vector<std::ranges::range_value_t<R>>> &&
             std::convertible_to<
                 std::invoke_result_t<Write&, std::vector<std::ranges::range_value_t<R>>>,
                 std::size_t>
std::size_t write_in_batches(R&& rows, std::size_t size, Write write) {
    if (size == 0) {
        throw std::invalid_argument("write_in_batches: size must be > 0");
    }
    std::size_t total = 0;
    auto first = std::ranges::begin(rows);
    const auto last = std::ranges::end(rows);
    while (first != last) {
        std::vector<std::ranges::range_value_t<R>> batch;
        batch.reserve(size);
        while (batch.size() < size && first != last) {
            batch.push_back(*first);
            ++first;
        }
        total += static_cast<std::size_t>(write(std::move(batch)));
    }
    return total;
}

/// Mean of each full window of `window` consecutive values; partial windows are never emitted.
/// The range is taken by value, so the result owns its input. `window == 0` throws
/// std::invalid_argument immediately. Lazy.
template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_value_t<R>, double>
Generator<double> moving_average(R values, std::size_t window) {
    if (window == 0) {
        throw std::invalid_argument("moving_average: window must be > 0");
    }
    return detail::moving_average_gen(std::move(values), window);
}

}  // namespace cppref::iterators_and_ranges
