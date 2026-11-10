#pragma once

#include <stdexcept>
#include <utility>

namespace cppref::raii_and_memory {

// Scope guards: run a callable when the enclosing scope ends. Each is move-only; a moved-from
// guard is inert. Callbacks run inside destructors, so they must not throw (an exception
// escaping a destructor terminates the program).
//
// Engineer-owned: store the callable plus an armed flag. scope_fail and scope_success also
// store std::uncaught_exceptions() as observed at construction, and compare it in the destructor.

// Runs on every exit unless released or moved from.
template <class F>
class scope_exit {
public:
    explicit scope_exit(F f) {
        (void)f;
        throw std::logic_error("not implemented: scope_exit");
    }
    scope_exit(const scope_exit&) = delete;
    scope_exit& operator=(const scope_exit&) = delete;
    scope_exit& operator=(scope_exit&&) = delete;
    scope_exit(scope_exit&& other) noexcept { (void)other; }
    ~scope_exit() = default;

    // Disarms the guard: the callable will not run.
    void release() noexcept {}
};

// Runs only when the scope is left by an exception.
template <class F>
class scope_fail {
public:
    explicit scope_fail(F f) {
        (void)f;
        throw std::logic_error("not implemented: scope_fail");
    }
    scope_fail(const scope_fail&) = delete;
    scope_fail& operator=(const scope_fail&) = delete;
    scope_fail& operator=(scope_fail&&) = delete;
    scope_fail(scope_fail&& other) noexcept { (void)other; }
    ~scope_fail() = default;

    void release() noexcept {}
};

// Runs only when the scope is left normally (no exception in flight).
template <class F>
class scope_success {
public:
    explicit scope_success(F f) {
        (void)f;
        throw std::logic_error("not implemented: scope_success");
    }
    scope_success(const scope_success&) = delete;
    scope_success& operator=(const scope_success&) = delete;
    scope_success& operator=(scope_success&&) = delete;
    scope_success(scope_success&& other) noexcept { (void)other; }
    ~scope_success() = default;

    void release() noexcept {}
};

}  // namespace cppref::raii_and_memory
