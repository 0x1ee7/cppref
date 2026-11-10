// Part 7a - lazy proxy: builds an expensive client on first use via operator->.
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <stdexcept>

namespace cppref::polymorphism {

/// Single-threaded lazy owner of a T.
///
/// - The factory is not called on construction.
/// - operator-> and operator* build the target on first use, then reuse it.
///   They are const: the proxy is logically const while the cache is filled.
/// - A factory exception propagates, leaves the proxy unloaded, and does not
///   count as an initialisation. A factory returning nullptr throws std::runtime_error.
/// - reset() discards the target. The next access calls the factory again.
///
/// Engineer: add the factory, the target (mutable unique_ptr) and the init counter
/// (mutable).
template <class T>
class Lazy {
public:
    using Factory = std::function<std::unique_ptr<T>()>;

    explicit Lazy(Factory /*factory*/) { throw std::logic_error("not implemented: Lazy::Lazy"); }

    [[nodiscard]] T* operator->() const { throw std::logic_error("not implemented: Lazy::operator->"); }

    [[nodiscard]] T& operator*() const { throw std::logic_error("not implemented: Lazy::operator*"); }

    /// True while a target is held.
    [[nodiscard]] bool loaded() const { throw std::logic_error("not implemented: Lazy::loaded"); }

    void reset() { throw std::logic_error("not implemented: Lazy::reset"); }

    /// Number of successful factory calls so far.
    [[nodiscard]] std::size_t init_count() const { throw std::logic_error("not implemented: Lazy::init_count"); }
};

}  // namespace cppref::polymorphism
