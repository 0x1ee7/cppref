#pragma once

#include <expected>
#include <exception>
#include <functional>
#include <ranges>
#include <type_traits>
#include <utility>
#include <vector>

// Part 5 · Exception-free helpers (reference solution).
namespace cppref::functional {

/// Calls fn(args...) and converts a thrown exception into an error value.
template <typename F, typename... Args>
auto try_call(F&& fn, Args&&... args)
    -> std::expected<std::invoke_result_t<F, Args...>, std::exception_ptr> {
    using R = std::invoke_result_t<F, Args...>;
    try {
        if constexpr (std::is_void_v<R>) {
            std::invoke(std::forward<F>(fn), std::forward<Args>(args)...);
            return {};
        } else {
            return std::invoke(std::forward<F>(fn), std::forward<Args>(args)...);
        }
    } catch (...) {
        return std::unexpected(std::current_exception());
    }
}

/// Turns a range of std::expected values into std::expected<std::vector<T>, E>.
/// Stops consuming the range at the first error.
template <std::ranges::input_range R>
auto collect(R&& range) -> std::expected<
    std::vector<typename std::ranges::range_value_t<R>::value_type>,
    typename std::ranges::range_value_t<R>::error_type> {
    using T = typename std::ranges::range_value_t<R>::value_type;
    using E = typename std::ranges::range_value_t<R>::error_type;
    std::vector<T> out;
    for (auto&& item : range) {
        if (!item.has_value()) {
            return std::unexpected<E>(item.error());
        }
        out.push_back(*item);
    }
    return out;
}

}  // namespace cppref::functional
