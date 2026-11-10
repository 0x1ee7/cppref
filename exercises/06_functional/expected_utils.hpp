#pragma once

#include <expected>
#include <exception>
#include <functional>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

// Part 5 · Exception-free helpers (engineer-owned).
namespace cppref::functional {

/// Calls fn(args...) and converts a thrown std::exception_ptr-capable exception
/// into an error value. Returns the callable's result on success.
/// Errors are stored as std::exception_ptr so the caller can rethrow or inspect.
template <typename F, typename... Args>
auto try_call(F&&, Args&&...)
    -> std::expected<std::invoke_result_t<F, Args...>, std::exception_ptr> {
    throw std::logic_error("not implemented: try_call");
}

/// Turns a range of std::expected values into std::expected<std::vector<T>, E>.
/// Returns the first error encountered and stops consuming the range there
/// (later elements must not be evaluated). Returns all values in order when
/// every element holds a value. An empty range yields an empty vector.
template <std::ranges::input_range R>
auto collect(R&&) -> std::expected<
    std::vector<typename std::ranges::range_value_t<R>::value_type>,
    typename std::ranges::range_value_t<R>::error_type> {
    throw std::logic_error("not implemented: collect");
}

}  // namespace cppref::functional
