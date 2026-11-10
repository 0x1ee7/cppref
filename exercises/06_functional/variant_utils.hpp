#pragma once

// Part 3 · Variant dispatch (engineer-owned).
//
// Implement the classic "overloaded" visitor helper in namespace
// cppref::functional:
//
//   template <typename... Ts> struct overloaded : Ts... { using Ts::operator()...; };
//   template <typename... Ts> overloaded(Ts...) -> overloaded<Ts...>;   // CTAD guide
//
// It is used with std::visit to dispatch on std::variant alternatives with one
// lambda per alternative (see cart.cpp and account.cpp). The name is declared
// here so the rest of the exercise compiles; define the template yourself.

namespace cppref::functional {

template <typename... Ts>
struct overloaded;

}  // namespace cppref::functional
