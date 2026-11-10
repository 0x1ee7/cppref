#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

// Part 2 · Persistent list (reference solution).
//
// Immutable singly linked list with structural sharing. Nodes are reference
// counted via shared_ptr<const Node>; the destructor unlinks uniquely owned
// nodes iteratively so that very long lists cannot overflow the stack.
namespace cppref::functional {

template <typename T>
class PersistentList {
    struct Node {
        T value;
        // mutable so the destructor can detach the tail of a uniquely owned node.
        mutable std::shared_ptr<const Node> next;
        std::size_t size;
    };

public:
    /// The empty list.
    PersistentList() = default;

    PersistentList(const PersistentList&) = default;
    PersistentList(PersistentList&&) noexcept = default;
    PersistentList& operator=(const PersistentList&) = default;
    PersistentList& operator=(PersistentList&&) noexcept = default;

    ~PersistentList() {
        // Only unlink nodes nobody else references; shared suffixes are released
        // normally (their refcount drops without recursion).
        while (head_ && head_.use_count() == 1) {
            auto next = std::move(head_->next);
            head_ = std::move(next);  // old head destroyed here with a null tail
        }
    }

    /// List whose front is values[0].
    [[nodiscard]] static PersistentList from_vector(const std::vector<T>& values) {
        PersistentList list;
        for (auto it = values.rbegin(); it != values.rend(); ++it) {
            list.head_ = make_node(*it, std::move(list.head_));
        }
        return list;
    }

    /// True when the list has no elements.
    [[nodiscard]] bool empty() const { return !head_; }

    /// Number of elements. O(1).
    [[nodiscard]] std::size_t size() const { return head_ ? head_->size : 0; }

    /// New list with `value` in front of *this. Shares all of *this.
    [[nodiscard]] PersistentList cons(T value) const {
        return PersistentList(make_node(std::move(value), head_));
    }

    /// First element. Throws std::out_of_range when empty.
    [[nodiscard]] const T& front() const {
        if (!head_) {
            throw std::out_of_range("front() on empty PersistentList");
        }
        return head_->value;
    }

    /// List without its first element. Throws std::out_of_range when empty.
    [[nodiscard]] PersistentList pop_front() const {
        if (!head_) {
            throw std::out_of_range("pop_front() on empty PersistentList");
        }
        return PersistentList(head_->next);
    }

    /// Element at `index`, or std::nullopt when out of range.
    [[nodiscard]] std::optional<T> get(std::size_t index) const {
        if (index >= size()) {
            return std::nullopt;
        }
        const Node* node = head_.get();
        for (std::size_t i = 0; i < index; ++i) {
            node = node->next.get();
        }
        return node->value;
    }

    /// New list with the element at `index` replaced; std::nullopt when
    /// `index >= size()`. Nodes after `index` are shared with *this.
    [[nodiscard]] std::optional<PersistentList> set(std::size_t index, T value) const {
        if (index >= size()) {
            return std::nullopt;
        }
        std::vector<T> prefix;
        prefix.reserve(index);
        const Node* node = head_.get();
        for (std::size_t i = 0; i < index; ++i) {
            prefix.push_back(node->value);
            node = node->next.get();
        }
        std::shared_ptr<const Node> rebuilt = make_node(std::move(value), node->next);
        for (auto it = prefix.rbegin(); it != prefix.rend(); ++it) {
            rebuilt = make_node(std::move(*it), std::move(rebuilt));
        }
        return PersistentList(std::move(rebuilt));
    }

    /// True when the node at `index` in *this and the node at `index` in
    /// `other` are the same object (not merely equal values). False when either
    /// list is shorter than `index + 1`.
    [[nodiscard]] bool shares_node_at(const PersistentList& other, std::size_t index) const {
        const Node* a = head_.get();
        const Node* b = other.head_.get();
        for (std::size_t i = 0; i < index; ++i) {
            if (!a || !b) {
                return false;
            }
            a = a->next.get();
            b = b->next.get();
        }
        return a != nullptr && a == b;
    }

    /// Elements in order, front first.
    [[nodiscard]] std::vector<T> to_vector() const {
        std::vector<T> out;
        out.reserve(size());
        for (const Node* node = head_.get(); node; node = node->next.get()) {
            out.push_back(node->value);
        }
        return out;
    }

    /// New list with `f` applied to each element; the element type may change.
    template <typename F>
    [[nodiscard]] auto map(F&& f) const -> PersistentList<std::invoke_result_t<F&, const T&>> {
        using U = std::invoke_result_t<F&, const T&>;
        std::vector<U> out;
        out.reserve(size());
        for (const Node* node = head_.get(); node; node = node->next.get()) {
            out.push_back(std::invoke(f, node->value));
        }
        return PersistentList<U>::from_vector(out);
    }

    /// Element-wise equality.
    [[nodiscard]] bool operator==(const PersistentList& other) const {
        if (size() != other.size()) {
            return false;
        }
        const Node* a = head_.get();
        const Node* b = other.head_.get();
        while (a && b) {
            if (a == b) {
                return true;  // shared suffix: identical from here on
            }
            if (!(a->value == b->value)) {
                return false;
            }
            a = a->next.get();
            b = b->next.get();
        }
        return true;
    }

private:
    explicit PersistentList(std::shared_ptr<const Node> head) : head_(std::move(head)) {}

    static std::shared_ptr<const Node> make_node(T value, std::shared_ptr<const Node> next) {
        const std::size_t size = next ? next->size + 1 : 1;
        return std::make_shared<Node>(Node{std::move(value), std::move(next), size});
    }

    std::shared_ptr<const Node> head_;
};

}  // namespace cppref::functional
