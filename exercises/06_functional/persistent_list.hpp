#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <vector>

// Part 2 · Persistent list (engineer-owned).
//
// An immutable singly linked list with structural sharing. Every "update"
// returns a new list and leaves *this untouched; unchanged suffixes are shared
// between versions through std::shared_ptr<const Node>.
//
// Engineer: add private data (for example a shared_ptr<const Node> head, where
// Node holds value, next and size) and implement every member. Things the
// tests check:
//  * copying a list is O(1) and never copies elements;
//  * set(i, v) copies only nodes 0..i and shares the rest;
//  * destroying a list with a very large number of nodes must not recurse
//    once per node (naive shared_ptr chains overflow the stack).
namespace cppref::functional {

template <typename T>
class PersistentList {
public:
    /// The empty list.
    PersistentList() = default;

    /// List whose front is values[0].
    [[nodiscard]] static PersistentList from_vector(const std::vector<T>&) {
        throw std::logic_error("not implemented: PersistentList::from_vector");
    }

    /// True when the list has no elements.
    [[nodiscard]] bool empty() const {
        throw std::logic_error("not implemented: PersistentList::empty");
    }

    /// Number of elements. O(1).
    [[nodiscard]] std::size_t size() const {
        throw std::logic_error("not implemented: PersistentList::size");
    }

    /// New list with `value` in front of *this. Shares all of *this.
    [[nodiscard]] PersistentList cons(T) const {
        throw std::logic_error("not implemented: PersistentList::cons");
    }

    /// First element. Throws std::out_of_range when empty.
    [[nodiscard]] const T& front() const {
        throw std::logic_error("not implemented: PersistentList::front");
    }

    /// List without its first element. Throws std::out_of_range when empty.
    [[nodiscard]] PersistentList pop_front() const {
        throw std::logic_error("not implemented: PersistentList::pop_front");
    }

    /// Element at `index`, or std::nullopt when out of range.
    [[nodiscard]] std::optional<T> get(std::size_t) const {
        throw std::logic_error("not implemented: PersistentList::get");
    }

    /// New list with the element at `index` replaced; std::nullopt when
    /// `index >= size()`. Nodes after `index` are shared with *this.
    [[nodiscard]] std::optional<PersistentList> set(std::size_t, T) const {
        throw std::logic_error("not implemented: PersistentList::set");
    }

    /// True when the node at `index` in *this and the node at `index` in
    /// `other` are the same object (not merely equal values). False when either
    /// list is shorter than `index + 1`.
    [[nodiscard]] bool shares_node_at(const PersistentList&, std::size_t) const {
        throw std::logic_error("not implemented: PersistentList::shares_node_at");
    }

    /// Elements in order, front first.
    [[nodiscard]] std::vector<T> to_vector() const {
        throw std::logic_error("not implemented: PersistentList::to_vector");
    }

    /// New list with `f` applied to each element; the element type may change.
    template <typename F>
    [[nodiscard]] auto map(F&&) const -> PersistentList<std::invoke_result_t<F&, const T&>> {
        throw std::logic_error("not implemented: PersistentList::map");
    }

    /// Element-wise equality.
    [[nodiscard]] bool operator==(const PersistentList&) const {
        throw std::logic_error("not implemented: PersistentList::operator==");
    }
};

}  // namespace cppref::functional
