#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <stop_token>
#include <utility>

namespace cppref::concurrency {

// A bounded multi-producer multi-consumer queue with close and cooperative cancellation.
//
// - push() blocks while the queue is full; pop() blocks while it is empty.
// - close(): no further pushes are accepted; pops keep draining buffered items, then return nullopt.
// - A stop request on the token passed to push()/pop() wakes the call immediately:
//   push() returns false and pop() returns nullopt, without consuming or losing data.
// - Throws std::invalid_argument if capacity == 0.
template <typename T>
class BoundedQueue {
public:
    explicit BoundedQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity == 0) throw std::invalid_argument("BoundedQueue: capacity must be >= 1");
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    // Returns true when the item was enqueued; false when the queue is closed or stop was requested.
    bool push(T value, std::stop_token stop = {}) {
        // Registered before taking the lock: the callback takes the lock itself, so it must
        // never run on a thread that already holds it. Destroyed after the lock is released.
        std::stop_callback wake(stop, [this] { wake_all(); });
        std::unique_lock lock(mutex_);
        not_full_.wait(lock, stop, [this, &stop] {
            return closed_ || stop.stop_requested() || items_.size() < capacity_;
        });
        if (closed_ || stop.stop_requested()) return false;
        items_.push_back(std::move(value));
        not_empty_.notify_one();
        return true;
    }

    // Returns the oldest item; nullopt when the queue is closed and drained, or stop was requested.
    std::optional<T> pop(std::stop_token stop = {}) {
        std::stop_callback wake(stop, [this] { wake_all(); });
        std::unique_lock lock(mutex_);
        not_empty_.wait(lock, stop, [this, &stop] {
            return closed_ || stop.stop_requested() || !items_.empty();
        });
        if (stop.stop_requested() || items_.empty()) return std::nullopt;
        T value = std::move(items_.front());
        items_.pop_front();
        not_full_.notify_one();
        return value;
    }

    // Idempotent. Wakes every blocked producer and consumer.
    void close() {
        {
            std::lock_guard lock(mutex_);
            closed_ = true;
        }
        not_full_.notify_all();
        not_empty_.notify_all();
    }

    bool closed() const {
        std::lock_guard lock(mutex_);
        return closed_;
    }

    std::size_t size() const {
        std::lock_guard lock(mutex_);
        return items_.size();
    }

    std::size_t capacity() const { return capacity_; }

private:
    // Taking the lock before notifying closes the window where a waiter has checked its
    // predicate but not yet started waiting, so the wakeup cannot be lost.
    void wake_all() {
        { std::lock_guard lock(mutex_); }
        not_full_.notify_all();
        not_empty_.notify_all();
    }

    mutable std::mutex mutex_;
    std::condition_variable_any not_full_;
    std::condition_variable_any not_empty_;
    std::deque<T> items_;
    bool closed_ = false;
    const std::size_t capacity_;
};

}  // namespace cppref::concurrency
