#pragma once

#include <stdexcept>

// Part 1 · Composition (engineer-owned).
//
//   compose(f, g, h)(x...)          == f(g(h(x...)))          right to left
//   pipe(value, f, g, h)            == h(g(f(value)))         left to right
//   partial_right(f, b...)(x...)    == f(x..., b...)
//
// Requirements:
//  * variadic templates, perfect forwarding of arguments;
//  * callables are stored by decayed value, so the result may outlive the
//    expression that created it (see the ComposeOwnsItsCallables test);
//  * pipe() returns whatever the last step returns (types may change);
//  * compose() with no arguments is the identity function.
namespace cppref::functional {

template <typename... Fs>
auto compose(Fs&&...) {
    return [](auto&&...) -> int { throw std::logic_error("not implemented: compose"); };
}

template <typename T, typename... Fs>
int pipe(T&&, Fs&&...) {
    throw std::logic_error("not implemented: pipe");
}

template <typename F, typename... Bound>
auto partial_right(F&&, Bound&&...) {
    return [](auto&&...) -> int { throw std::logic_error("not implemented: partial_right"); };
}

}  // namespace cppref::functional
