#pragma once

#include <cstddef>
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
    explicit BoundedQueue(std::size_t capacity) {
        (void)capacity;
        throw std::logic_error("not implemented: BoundedQueue::BoundedQueue");
    }

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;

    // Returns true when the item was enqueued; false when the queue is closed or stop was requested.
    bool push(T value, std::stop_token stop = {}) {
        (void)value;
        (void)stop;
        throw std::logic_error("not implemented: BoundedQueue::push");
    }

    // Returns the oldest item; nullopt when the queue is closed and drained, or stop was requested.
    std::optional<T> pop(std::stop_token stop = {}) {
        (void)stop;
        throw std::logic_error("not implemented: BoundedQueue::pop");
    }

    // Idempotent. Wakes every blocked producer and consumer.
    void close() {
        throw std::logic_error("not implemented: BoundedQueue::close");
    }

    bool closed() const {
        throw std::logic_error("not implemented: BoundedQueue::closed");
    }

    std::size_t size() const {
        throw std::logic_error("not implemented: BoundedQueue::size");
    }

    std::size_t capacity() const {
        throw std::logic_error("not implemented: BoundedQueue::capacity");
    }

private:
    // Private members for the engineer to add: deque, mutex, two condition_variable_any, closed flag.
    // Hint: register a std::stop_callback BEFORE taking the lock, so the callback cannot self-deadlock.
};

}  // namespace cppref::concurrency
