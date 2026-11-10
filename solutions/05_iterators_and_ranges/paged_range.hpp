// Part 3: a lazily paginated input range over an injected fetch function.
#pragma once

#include <concepts>
#include <cstddef>
#include <iterator>
#include <optional>
#include <ranges>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace cppref::iterators_and_ranges {

/// One page of results. `next_cursor == std::nullopt` marks the last page.
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
template <typename T, typename Fetch>
class PagedRange : public std::ranges::view_interface<PagedRange<T, Fetch>> {
public:
    class iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using iterator_concept = std::input_iterator_tag;
        using iterator_category = std::input_iterator_tag;

        const T& operator*() const { return owner_->current(); }
        iterator& operator++() {
            ++owner_->index_;
            return *this;
        }
        void operator++(int) { ++*this; }
        bool operator==(std::default_sentinel_t) const { return !owner_->ensure_item(); }

    private:
        friend class PagedRange;
        explicit iterator(PagedRange* owner) : owner_(owner) {}

        PagedRange* owner_ = nullptr;
    };

    PagedRange(Fetch fetch, std::optional<std::string> start)
        : fetch_(std::move(fetch)), cursor_(std::move(start)) {}

    iterator begin() { return iterator(this); }
    [[nodiscard]] std::default_sentinel_t end() const { return {}; }

private:
    // True when an item is available at `index_`, fetching (and skipping empty pages) as needed.
    bool ensure_item() const {
        while (index_ >= items_.size()) {
            if (!more_) {
                return false;
            }
            load_next_page();
        }
        return true;
    }

    const T& current() const {
        if (!ensure_item()) {
            throw std::logic_error("dereferenced end of paginated range");
        }
        return items_[index_];
    }

    void load_next_page() const {
        if (requested_.contains(cursor_)) {
            throw PaginationLoopError("pagination cursor requested twice; the server is looping");
        }
        Page<T> page = fetch_(cursor_);
        requested_.insert(cursor_);
        items_ = std::move(page.items);
        index_ = 0;
        more_ = page.next_cursor.has_value();
        cursor_ = std::move(page.next_cursor);
    }

    // Fetching happens from const operator* / operator==, so the state is mutable.
    mutable Fetch fetch_;
    mutable std::optional<std::string> cursor_;
    mutable bool more_ = true;
    mutable std::vector<T> items_;
    mutable std::size_t index_ = 0;
    mutable std::set<std::optional<std::string>> requested_;
};

/// Lazily paginated items. `fetch(cursor)` is called with `std::nullopt` for the first page,
/// or with the cursor from the previous page; `start` replaces the first cursor (resume).
template <typename Fetch>
    requires std::invocable<Fetch&, const std::optional<std::string>&>
PagedRange<page_item_t<Fetch>, Fetch> paginate(Fetch fetch,
                                               std::optional<std::string> start = std::nullopt) {
    return PagedRange<page_item_t<Fetch>, Fetch>(std::move(fetch), std::move(start));
}

}  // namespace cppref::iterators_and_ranges
