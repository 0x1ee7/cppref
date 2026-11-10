#include "thread_pool.hpp"

#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <stop_token>
#include <thread>
#include <utility>

namespace cppref::concurrency {

ThreadPool::ThreadPool(std::size_t threads) {
    if (threads == 0) throw std::invalid_argument("ThreadPool: threads must be >= 1");
    workers_.reserve(threads);
    for (std::size_t i = 0; i < threads; ++i) {
        // jthread supplies its own stop token, which the destructor and cancel both trigger.
        workers_.emplace_back([this](std::stop_token stop) { worker_loop(stop); });
    }
}

ThreadPool::~ThreadPool() {
    shutdown(ShutdownMode::drain);
}

void ThreadPool::enqueue(std::function<void()> job) {
    {
        std::lock_guard lock(mutex_);
        if (!accepting_) throw std::runtime_error("ThreadPool: submit after shutdown");
        queue_.push_back(std::move(job));
    }
    cv_.notify_one();
}

void ThreadPool::worker_loop(std::stop_token stop) {
    for (;;) {
        std::function<void()> job;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, stop, [this] { return !queue_.empty() || !accepting_; });
            // Cancel: stop was requested, so leave queued work to be dropped by shutdown().
            // Drain: nothing queued and no new work can arrive, so this worker is done.
            if (stop.stop_requested() || queue_.empty()) return;
            job = std::move(queue_.front());
            queue_.pop_front();
        }
        job();  // a packaged_task stores any exception in its future; nothing escapes here
    }
}

void ThreadPool::shutdown(ShutdownMode mode) {
    std::deque<std::function<void()>> dropped;
    {
        std::lock_guard lock(mutex_);
        if (!accepting_) return;
        accepting_ = false;
        if (mode == ShutdownMode::cancel) {
            dropped.swap(queue_);
            for (auto& worker : workers_) worker.request_stop();
        }
    }
    // Destroying unrun jobs outside the lock breaks their promises, so waiters wake promptly.
    dropped.clear();
    cv_.notify_all();
    for (auto& worker : workers_) {
        if (worker.joinable()) worker.join();
    }
}

std::size_t ThreadPool::pending() const {
    std::lock_guard lock(mutex_);
    return queue_.size();
}

}  // namespace cppref::concurrency
