// Part 4: a minimal coroutine-backed generator (std::generator is C++23 and
// missing from MSVC 19.38).
#pragma once

#include <coroutine>
#include <cstddef>
#include <exception>
#include <iterator>
#include <optional>
#include <utility>

namespace cppref::iterators_and_ranges {

/// Lazy single-pass sequence produced by a coroutine: the C++ analogue of a Python generator.
template <typename T>
class Generator {
public:
    struct promise_type {
        Generator get_return_object() {
            return Generator{std::coroutine_handle<promise_type>::from_promise(*this)};
        }
        std::suspend_always initial_suspend() noexcept { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        std::suspend_always yield_value(T value) {
            value_.emplace(std::move(value));
            return {};
        }
        void return_void() noexcept {}
        void unhandled_exception() { exception_ = std::current_exception(); }

        std::optional<T> value_;
        std::exception_ptr exception_;
    };

    Generator(Generator&& other) noexcept
        : handle_(std::exchange(other.handle_, nullptr)),
          started_(std::exchange(other.started_, false)) {}

    Generator& operator=(Generator&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
            started_ = std::exchange(other.started_, false);
        }
        return *this;
    }

    Generator(const Generator&) = delete;
    Generator& operator=(const Generator&) = delete;

    ~Generator() { reset(); }

    class iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = T;
        using iterator_concept = std::input_iterator_tag;
        using iterator_category = std::input_iterator_tag;

        T& operator*() const { return *handle_.promise().value_; }
        iterator& operator++() {
            advance(handle_);
            return *this;
        }
        void operator++(int) { advance(handle_); }
        bool operator==(std::default_sentinel_t) const { return !handle_ || handle_.done(); }

    private:
        friend class Generator;
        explicit iterator(std::coroutine_handle<promise_type> h) : handle_(h) {}

        std::coroutine_handle<promise_type> handle_;
    };

    iterator begin() {
        if (!started_) {
            started_ = true;
            advance(handle_);
        }
        return iterator{handle_};
    }

    [[nodiscard]] std::default_sentinel_t end() const { return {}; }

private:
    using handle_type = std::coroutine_handle<promise_type>;

    explicit Generator(handle_type h) noexcept : handle_(h) {}

    // Resumes the body once and rethrows any exception it escaped with.
    static void advance(handle_type h) {
        if (!h || h.done()) {
            return;
        }
        h.resume();
        if (auto error = std::exchange(h.promise().exception_, nullptr)) {
            std::rethrow_exception(error);
        }
    }

    void reset() noexcept {
        if (handle_) {
            handle_.destroy();
            handle_ = nullptr;
        }
    }

    handle_type handle_;
    bool started_ = false;
};

}  // namespace cppref::iterators_and_ranges
