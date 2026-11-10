// Engineer-owned. Part 4: a minimal coroutine-backed generator (std::generator is C++23 and
// missing from MSVC 19.38).
#pragma once

#include <coroutine>
#include <cstddef>
#include <exception>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace cppref::iterators_and_ranges {

/// Lazy single-pass sequence produced by a coroutine: the C++ analogue of a Python generator.
///
/// Contract:
/// - Calling a function that returns Generator<T> runs no user code until the first `begin()`.
/// - Each `co_yield v` makes `*it` return `v`; `++it` resumes the body.
/// - An exception escaping the body is rethrown to the consumer by `begin()` or `++`,
///   after which the generator is finished.
/// - Destroying the Generator (early break included) destroys the coroutine frame, so locals
///   still alive in the body are released. Normal completion releases them too.
/// - Move-only.
template <typename T>
class Generator {
public:
    struct promise_type {
        Generator get_return_object() {
            throw std::logic_error("not implemented: Generator::promise_type::get_return_object");
        }
        std::suspend_always initial_suspend() {
            throw std::logic_error("not implemented: Generator::promise_type::initial_suspend");
        }
        std::suspend_always final_suspend() noexcept {
            return {};  // unreachable in the stub: get_return_object() throws first
        }
        std::suspend_always yield_value(T) {
            throw std::logic_error("not implemented: Generator::promise_type::yield_value");
        }
        void return_void() {
            throw std::logic_error("not implemented: Generator::promise_type::return_void");
        }
        void unhandled_exception() {
            throw std::logic_error("not implemented: Generator::promise_type::unhandled_exception");
        }

        // engineer: current value slot, started flag, captured exception
    };

    Generator(Generator&&) {
        throw std::logic_error("not implemented: Generator move constructor");
    }
    Generator& operator=(Generator&&) {
        throw std::logic_error("not implemented: Generator move assignment");
    }
    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;
    ~Generator() noexcept(false) {
        throw std::logic_error("not implemented: Generator destructor");
    }

    class iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = T;

        T& operator*() const {
            throw std::logic_error("not implemented: Generator::iterator::operator*");
        }
        iterator& operator++() {
            throw std::logic_error("not implemented: Generator::iterator::operator++");
        }
        void operator++(int) {
            throw std::logic_error("not implemented: Generator::iterator::operator++(int)");
        }
        friend bool operator==(const iterator&, std::default_sentinel_t) {
            throw std::logic_error("not implemented: Generator::iterator == sentinel");
        }

        // engineer: coroutine handle
    };

    iterator begin() {
        throw std::logic_error("not implemented: Generator::begin");
    }
    [[nodiscard]] std::default_sentinel_t end() const {
        throw std::logic_error("not implemented: Generator::end");
    }

    // engineer: std::coroutine_handle<promise_type> handle_;
};

}  // namespace cppref::iterators_and_ranges
