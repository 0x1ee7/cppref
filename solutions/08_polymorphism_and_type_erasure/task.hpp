// Part 5 - type erasure: a move-only Task that holds any runnable, and a
// PropertyBag that stores heterogeneous values in std::any.
#pragma once

#include <any>
#include <concepts>
#include <cstddef>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace cppref::polymorphism {

/// A type is Runnable if it has a member run() or can be invoked with no arguments.
/// When both exist, run() wins.
template <class T>
concept Runnable = requires(T& t) { t.run(); } || std::invocable<T&>;

/// Type-erased, move-only unit of work.
///
/// Unlike std::function, it accepts move-only callables and objects with run().
/// Copying is deleted: a Task owns its callable exclusively.
class Task {
public:
    /// Rvalues are moved in. Lvalues are copied in.
    template <class T>
        requires(!std::same_as<std::remove_cvref_t<T>, Task>) && Runnable<std::remove_cvref_t<T>>
    Task(T&& task) : impl_(std::make_unique<Model<std::remove_cvref_t<T>>>(std::forward<T>(task))) {}

    Task(Task&& other) noexcept : impl_(std::move(other.impl_)) {}

    Task& operator=(Task&& other) noexcept {
        impl_ = std::move(other.impl_);
        return *this;
    }

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    ~Task() = default;

    /// Runs the callable. Repeated calls run it again, so state persists.
    /// Throws std::logic_error on a moved-from Task. Exceptions from the callable
    /// propagate, and the Task stays usable.
    void run() {
        if (!impl_) {
            throw std::logic_error("Task::run called on an empty Task");
        }
        impl_->invoke();
    }

    /// True after the Task has been moved from.
    [[nodiscard]] bool empty() const { return impl_ == nullptr; }

private:
    /// Type-erasure interface.
    struct Concept {
        virtual ~Concept() = default;
        virtual void invoke() = 0;
    };

    /// Holds the callable by value. Dispatch prefers run() over operator().
    template <class U>
    struct Model final : Concept {
        explicit Model(U value) : value_(std::move(value)) {}

        void invoke() override {
            if constexpr (requires(U& u) { u.run(); }) {
                value_.run();
            } else {
                value_();
            }
        }

        U value_;
    };

    std::unique_ptr<Concept> impl_;
};

/// Heterogeneous string-keyed property bag backed by std::any.
///
/// Typed lookups return a pointer to the stored value when the key exists and
/// holds exactly T. Otherwise they return nullptr. A type mismatch never throws.
class PropertyBag {
public:
    /// Stores a copy of `value` under `key`, replacing any previous value and type.
    template <class T>
    void set(std::string key, T value) {
        entries_.insert_or_assign(std::move(key), std::any(std::move(value)));
    }

    /// Pointer to the stored T, or nullptr when the key is missing or holds another type.
    template <class T>
    [[nodiscard]] const T* get(std::string_view key) const {
        const auto it = entries_.find(key);
        if (it == entries_.end()) {
            return nullptr;
        }
        return std::any_cast<T>(&it->second);
    }

    [[nodiscard]] bool contains(std::string_view key) const { return entries_.find(key) != entries_.end(); }

    /// Returns true when an entry was removed.
    bool erase(std::string_view key) {
        const auto it = entries_.find(key);
        if (it == entries_.end()) {
            return false;
        }
        entries_.erase(it);
        return true;
    }

    [[nodiscard]] std::size_t size() const { return entries_.size(); }

private:
    // Transparent comparator: find() accepts string_view without building a std::string.
    std::map<std::string, std::any, std::less<>> entries_;
};

}  // namespace cppref::polymorphism
