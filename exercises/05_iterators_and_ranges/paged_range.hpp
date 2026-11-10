// Engineer-owned. Part 3: a lazily paginated input range over an injected fetch function.
#pragma once

#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace cppref::iterators_and_ranges {

/// One page of results. `next_cursor == std::nullopt` marks the last page.
/// An empty `items` on a non-last page is legal.
template <typename T>
struct Page {
    using value_type = T;

    std::vector<T> items;
    std::optional<std::string> next_cursor;
};

/// Raised when a cursor would be requested a second time. The start cursor counts as requested.
class PaginationLoopError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/// Item type of the pages `Fetch` returns when called with `const std::optional<std::string>&`.
template <typename Fetch>
using page_item_t = typename std::remove_cvref_t<
    std::invoke_result_t<Fetch&, const std::optional<std::string>&>>::value_type;

/// Single-pass input range over the items of every page. Built by paginate().
///
/// Contract:
/// - Constructing the range performs no fetch.
/// - A fetch happens only when the consumer needs an item the current page does not hold
///   (the first `*it` or `it == end` fetches the first page; empty pages are skipped).
/// - `fetch` exceptions propagate to the consumer.
/// - A cursor that was already requested throws PaginationLoopError before it is requested again.
template <typename T, typename Fetch>
class PagedRange : public std::ranges::view_interface<PagedRange<T, Fetch>> {
public:
    class iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = T;

        const T& operator*() const {
            throw std::logic_error("not implemented: PagedRange::iterator::operator*");
        }
        iterator& operator++() {
            throw std::logic_error("not implemented: PagedRange::iterator::operator++");
        }
        void operator++(int) {
            throw std::logic_error("not implemented: PagedRange::iterator::operator++(int)");
        }
        friend bool operator==(const iterator&, std::default_sentinel_t) {
            throw std::logic_error("not implemented: PagedRange::iterator == sentinel");
        }

        // engineer: owner pointer, current page items, index, next cursor, pending flag.
        // Fetching happens from const operator* / operator==, so those members need `mutable`.
    };

    PagedRange(Fetch, std::optional<std::string>) {
        throw std::logic_error("not implemented: PagedRange");
    }

    iterator begin() {
        throw std::logic_error("not implemented: PagedRange::begin");
    }
    [[nodiscard]] std::default_sentinel_t end() const {
        throw std::logic_error("not implemented: PagedRange::end");
    }

    // engineer: fetch callable, start cursor, set of requested cursors
};

/// Lazily paginated items. `fetch(cursor)` is called with `std::nullopt` for the first page,
/// or with the cursor from the previous page; `start` replaces the first cursor (resume).
template <typename Fetch>
    requires std::invocable<Fetch&, const std::optional<std::string>&>
PagedRange<page_item_t<Fetch>, Fetch> paginate(Fetch, std::optional<std::string> = std::nullopt) {
    throw std::logic_error("not implemented: paginate");
}

}  // namespace cppref::iterators_and_ranges
