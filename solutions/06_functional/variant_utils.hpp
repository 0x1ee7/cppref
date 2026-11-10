#pragma once

// Part 3 · Variant dispatch (reference solution).

namespace cppref::functional {

/// Several lambdas folded into one visitor for std::visit.
template <typename... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};

template <typename... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

}  // namespace cppref::functional
