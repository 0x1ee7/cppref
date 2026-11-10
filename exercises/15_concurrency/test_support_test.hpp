#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

namespace cppref::concurrency::testing {

// Counts down to zero; wait_for() returns false if the bound elapses first.
class TimedLatch {
public:
    explicit TimedLatch(std::ptrdiff_t count) : count_(count) {}

    void count_down() {
        std::lock_guard lock(mutex_);
        if (count_ > 0 && --count_ == 0) cv_.notify_all();
    }

    bool wait_for(std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [this] { return count_ == 0; });
    }

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::ptrdiff_t count_;
};

// Runs fn on a thread; join() rethrows any exception the body threw.
class Background {
public:
    template <typename F>
    explicit Background(F&& fn)
        : thread_([this, f = std::forward<F>(fn)]() mutable {
              try {
                  f();
              } catch (...) {
                  error_ = std::current_exception();
              }
          }) {}

    Background(const Background&) = delete;
    Background& operator=(const Background&) = delete;

    void join() {
        if (thread_.joinable()) thread_.join();
        if (error_) std::rethrow_exception(error_);
    }

    ~Background() {
        if (thread_.joinable()) thread_.join();
    }

private:
    std::exception_ptr error_;
    std::jthread thread_;
};

// Runs fn(i) for i in [0, n) on n threads and joins them all; rethrows the lowest-index exception.
template <typename F>
void parallel_for(std::size_t n, F fn) {
    std::vector<std::exception_ptr> errors(n);
    std::vector<std::jthread> threads;
    threads.reserve(n);
    for (std::size_t i = 0; i < n; ++i) {
        threads.emplace_back([&, i] {
            try {
                fn(i);
            } catch (...) {
                errors[i] = std::current_exception();
            }
        });
    }
    for (auto& t : threads) t.join();
    for (auto& e : errors) {
        if (e) std::rethrow_exception(e);
    }
}

}  // namespace cppref::concurrency::testing
