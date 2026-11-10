#pragma once

#include <cstddef>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

// Part 1 · Composition (reference solution).
//
//   compose(f, g, h)(x...)          == f(g(h(x...)))          right to left
//   pipe(value, f, g, h)            == h(g(f(value)))         left to right
//   partial_right(f, b...)(x...)    == f(x..., b...)
namespace cppref::functional {

namespace detail {

template <typename F, typename Tuple, typename... Args>
auto invoke_with_suffix(const F& f, const Tuple& bound, Args&&... args) {
    return std::apply(
        [&](const auto&... bs) { return std::invoke(f, std::forward<Args>(args)..., bs...); },
        bound);
}

template <typename A>
auto identity(A&& a) {
    return std::forward<A>(a);
}

}  // namespace detail

/// Right-to-left composition of value-stored callables. Only the innermost
/// (last) callable receives the full argument pack; every other step receives
/// the single result of the step to its right.
template <typename... Fs>
class Composed {
public:
    explicit Composed(Fs... fs) : fs_(std::move(fs)...) {}

    template <typename... Args>
    auto operator()(Args&&... args) const {
        if constexpr (sizeof...(Fs) == 0) {
            static_assert(sizeof...(Args) == 1, "identity takes exactly one argument");
            return detail::identity(std::forward<Args>(args)...);
        } else {
            return eval<0>(std::forward<Args>(args)...);
        }
    }

private:
    template <std::size_t I, typename... Args>
    auto eval(Args&&... args) const {
        if constexpr (I + 1 == sizeof...(Fs)) {
            return std::get<I>(fs_)(std::forward<Args>(args)...);
        } else {
            return std::get<I>(fs_)(eval<I + 1>(std::forward<Args>(args)...));
        }
    }

    std::tuple<Fs...> fs_;
};

template <typename... Fs>
auto compose(Fs&&... fs) {
    return Composed<std::decay_t<Fs>...>(std::forward<Fs>(fs)...);
}

/// Left-to-right threading of a value through callables. Returns whatever the
/// last step returns. pipe(v) is just v.
template <typename T>
auto pipe(T&& value) {
    return std::forward<T>(value);
}

template <typename T, typename F, typename... Fs>
auto pipe(T&& value, F&& f, Fs&&... fs) {
    if constexpr (sizeof...(Fs) == 0) {
        return std::invoke(std::forward<F>(f), std::forward<T>(value));
    } else {
        return pipe(std::invoke(std::forward<F>(f), std::forward<T>(value)), std::forward<Fs>(fs)...);
    }
}

template <typename F, typename Tuple>
class PartialRight {
public:
    PartialRight(F f, Tuple bound) : f_(std::move(f)), bound_(std::move(bound)) {}

    template <typename... Args>
    auto operator()(Args&&... args) const {
        return detail::invoke_with_suffix(f_, bound_, std::forward<Args>(args)...);
    }

private:
    F f_;
    Tuple bound_;
};

/// Binds trailing arguments: partial_right(f, b)(x) == f(x, b).
template <typename F, typename... Bound>
auto partial_right(F&& f, Bound&&... bound) {
    using Tuple = std::tuple<std::decay_t<Bound>...>;
    return PartialRight<std::decay_t<F>, Tuple>(std::forward<F>(f),
                                                Tuple(std::forward<Bound>(bound)...));
}

}  // namespace cppref::functional
