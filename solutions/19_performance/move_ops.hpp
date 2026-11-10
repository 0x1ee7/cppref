// Part 4: move semantics and reallocation policy, proven with instrumented element types.
#pragma once

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <iterator>
#include <span>
#include <type_traits>
#include <vector>

namespace cppref::performance {

// Removes the first min(max, pending.size()) elements from `pending` and returns them, in order,
// in a new vector. Elements are moved, never copied. The result is reserved once, so it makes
// exactly one allocation when it is non-empty and none when it is empty.
template <std::movable T>
std::vector<T> drain_front(std::vector<T>& pending, std::size_t max) {
    const std::size_t n = std::min(max, pending.size());
    std::vector<T> out;
    if (n == 0) return out;
    out.reserve(n);
    const auto last = pending.begin() + static_cast<std::ptrdiff_t>(n);
    out.insert(out.end(), std::make_move_iterator(pending.begin()), std::make_move_iterator(last));
    pending.erase(pending.begin(), last);
    return out;
}

// Appends `count` elements, each constructed as T(args...). Reserves once for dst.size() + count,
// so no reallocation happens while the elements are appended.
template <class T, class... Args>
void emplace_n(std::vector<T>& dst, std::size_t count, const Args&... args) {
    if (count == 0) return;
    dst.reserve(dst.size() + count);
    for (std::size_t i = 0; i < count; ++i) dst.emplace_back(args...);
}

// Appends copies of `items` to `dst` with the strong guarantee: copies are staged first, so a
// throwing copy leaves `dst` untouched. The final move into `dst` needs a nothrow move, which is
// what makes the append itself non-throwing once the capacity is reserved.
template <std::copyable T>
void append_strong(std::vector<T>& dst, std::span<const T> items) {
    static_assert(std::is_nothrow_move_constructible_v<T>,
                  "append_strong needs a nothrow move for its strong guarantee");
    std::vector<T> staged;
    staged.reserve(items.size());
    for (const T& item : items) staged.push_back(item);  // may throw: dst is still untouched

    dst.reserve(dst.size() + staged.size());  // strong: vector::reserve keeps dst on failure
    for (T& item : staged) dst.push_back(std::move(item));  // capacity reserved: no reallocation
}

}  // namespace cppref::performance
