// Part 4: move semantics and reallocation policy, proven with instrumented element types.
#pragma once

#include <concepts>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

namespace cppref::performance {

// Removes the first min(max, pending.size()) elements from `pending` and returns them, in order,
// in a new vector. Elements are moved, never copied. The result is reserved once, so it makes
// exactly one allocation when it is non-empty and none when it is empty. `pending` keeps its
// remaining elements in their original order.
template <std::movable T>
std::vector<T> drain_front(std::vector<T>& /*pending*/, std::size_t /*max*/) {
    throw std::logic_error("not implemented: drain_front");
}

// Appends `count` elements, each constructed as T(args...) (every argument is passed as a const
// lvalue). Reserves once for dst.size() + count, so existing elements are never relocated.
template <class T, class... Args>
void emplace_n(std::vector<T>& /*dst*/, std::size_t /*count*/, const Args&... /*args*/) {
    throw std::logic_error("not implemented: emplace_n");
}

// Appends copies of `items` to `dst` with the strong guarantee: if any copy throws, `dst` is left
// exactly as it was. Existing elements are relocated with std::move_if_noexcept, so they are moved
// when T's move is noexcept and copied otherwise. Each item is copied exactly once when T is
// nothrow-movable.
template <std::copyable T>
void append_strong(std::vector<T>& /*dst*/, std::span<const T> /*items*/) {
    throw std::logic_error("not implemented: append_strong");
}

}  // namespace cppref::performance
