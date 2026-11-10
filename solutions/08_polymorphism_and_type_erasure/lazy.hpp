// Part 7a - lazy proxy: builds an expensive client on first use via operator->.
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>
#include <utility>

namespace cppref::polymorphism {

/// Single-threaded lazy owner of a T.
///
/// - The factory is not called on construction.
/// - operator-> and operator* build the target on first use, then reuse it.
///   They are const: the proxy is logically const while the cache is filled.
/// - A factory exception propagates, leaves the proxy unloaded, and does not
///   count as an initialisation. A factory returning nullptr throws std::runtime_error.
/// - reset() discards the target. The next access calls the factory again.
template <class T>
class Lazy {
public:
    using Factory = std::function<std::unique_ptr<T>()>;

    explicit Lazy(Factory factory) : factory_(std::move(factory)) {}

    [[nodiscard]] T* operator->() const { return &ensure(); }

    [[nodiscard]] T& operator*() const { return ensure(); }

    /// True while a target is held.
    [[nodiscard]] bool loaded() const { return target_ != nullptr; }

    void reset() { target_.reset(); }

    /// Number of successful factory calls so far.
    [[nodiscard]] std::size_t init_count() const { return init_count_; }

private:
    T& ensure() const {
        if (!target_) {
            // If the factory throws, nothing below runs: the proxy stays unloaded.
            std::unique_ptr<T> made = factory_();
            if (!made) {
                throw std::runtime_error("Lazy factory returned nullptr");
            }
            target_ = std::move(made);
            ++init_count_;
        }
        return *target_;
    }

    Factory factory_;
    mutable std::unique_ptr<T> target_;
    mutable std::size_t init_count_ = 0;
};

}  // namespace cppref::polymorphism
