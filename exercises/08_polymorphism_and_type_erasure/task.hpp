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
    Task(T&& /*task*/) {
        throw std::logic_error("not implemented: Task::Task(T&&)");
    }

    Task(Task&& /*other*/) { throw std::logic_error("not implemented: Task(Task&&)"); }
    Task& operator=(Task&& /*other*/) { throw std::logic_error("not implemented: Task::operator=(Task&&)"); }
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;
    ~Task() = default;

    /// Runs the callable. Repeated calls run it again, so state persists.
    /// Throws std::logic_error on a moved-from Task. Exceptions from the callable
    /// propagate, and the Task stays usable.
    void run() { throw std::logic_error("not implemented: Task::run"); }

    /// True after the Task has been moved from.
    [[nodiscard]] bool empty() const { throw std::logic_error("not implemented: Task::empty"); }

private:
    /// Type-erasure interface. Engineer: implement the template Model<U> that
    /// derives from it, and choose how the target is stored.
    struct Concept {
        virtual ~Concept() = default;
        virtual void invoke() = 0;
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
    void set(std::string /*key*/, T /*value*/) {
        throw std::logic_error("not implemented: PropertyBag::set");
    }

    /// Pointer to the stored T, or nullptr when the key is missing or holds another type.
    template <class T>
    [[nodiscard]] const T* get(std::string_view /*key*/) const {
        throw std::logic_error("not implemented: PropertyBag::get");
    }

    [[nodiscard]] bool contains(std::string_view /*key*/) const {
        throw std::logic_error("not implemented: PropertyBag::contains");
    }

    /// Returns true when an entry was removed.
    bool erase(std::string_view /*key*/) { throw std::logic_error("not implemented: PropertyBag::erase"); }

    [[nodiscard]] std::size_t size() const { throw std::logic_error("not implemented: PropertyBag::size"); }

    // Engineer: data members (key -> std::any).
};

}  // namespace cppref::polymorphism
