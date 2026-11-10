#pragma once

#include <exception>
#include <type_traits>
#include <utility>

namespace cppref::raii_and_memory {

// Scope guards run a callable when the enclosing scope ends. They are move-only; a moved-from
// guard is inert. The callable runs inside a destructor, so it must not throw.
//
// scope_fail / scope_success compare std::uncaught_exceptions() at destruction with the value
// captured at construction. This detects "unwinding through this scope" correctly, even when an
// exception was thrown and caught inside the scope (a plain try/catch-based check would not).

template <class F>
class scope_exit {
public:
    explicit scope_exit(F f) : f_(std::move(f)) {}
    scope_exit(const scope_exit&) = delete;
    scope_exit& operator=(const scope_exit&) = delete;
    scope_exit& operator=(scope_exit&&) = delete;
    scope_exit(scope_exit&& other) noexcept(std::is_nothrow_move_constructible_v<F>)
        : f_(std::move(other.f_)), armed_(std::exchange(other.armed_, false)) {}
    ~scope_exit() {
        if (armed_) f_();
    }

    void release() noexcept { armed_ = false; }

private:
    F f_;
    bool armed_ = true;
};

template <class F>
class scope_fail {
public:
    explicit scope_fail(F f) : f_(std::move(f)), uncaught_(std::uncaught_exceptions()) {}
    scope_fail(const scope_fail&) = delete;
    scope_fail& operator=(const scope_fail&) = delete;
    scope_fail& operator=(scope_fail&&) = delete;
    scope_fail(scope_fail&& other) noexcept(std::is_nothrow_move_constructible_v<F>)
        : f_(std::move(other.f_)), uncaught_(other.uncaught_), armed_(std::exchange(other.armed_, false)) {}
    ~scope_fail() {
        if (armed_ && std::uncaught_exceptions() > uncaught_) f_();
    }

    void release() noexcept { armed_ = false; }

private:
    F f_;
    int uncaught_;
    bool armed_ = true;
};

template <class F>
class scope_success {
public:
    explicit scope_success(F f) : f_(std::move(f)), uncaught_(std::uncaught_exceptions()) {}
    scope_success(const scope_success&) = delete;
    scope_success& operator=(const scope_success&) = delete;
    scope_success& operator=(scope_success&&) = delete;
    scope_success(scope_success&& other) noexcept(std::is_nothrow_move_constructible_v<F>)
        : f_(std::move(other.f_)), uncaught_(other.uncaught_), armed_(std::exchange(other.armed_, false)) {}
    ~scope_success() {
        if (armed_ && std::uncaught_exceptions() <= uncaught_) f_();
    }

    void release() noexcept { armed_ = false; }

private:
    F f_;
    int uncaught_;
    bool armed_ = true;
};

}  // namespace cppref::raii_and_memory
