#pragma once

#include <atomic>
#include <cstddef>
#include <new>
#include <optional>
#include <utility>

namespace cppref::concurrency {

// A lock-free single-producer single-consumer ring buffer. Exactly one thread may call try_push,
// and exactly one (possibly different) thread may call try_pop.
//
// - Capacity is a compile-time power of two, so indices wrap with a mask.
// - One slot is never used as a sentinel: the ring holds exactly Capacity items.
// - head and tail live on separate cache lines to avoid false sharing.
// - Destroying the ring destroys any items still stored in it.
template <typename T, std::size_t Capacity>
class SpscRing {
    static_assert(Capacity >= 1 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be a power of two");

public:
    SpscRing() = default;
    SpscRing(const SpscRing&) = delete;
    SpscRing& operator=(const SpscRing&) = delete;

    ~SpscRing() {
        while (try_pop()) {
        }
    }

    // Returns false (leaving `value` untouched) when the ring is full.
    bool try_push(T&& value) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);  // only we write tail
        const std::size_t head = head_.load(std::memory_order_acquire);
        if (tail - head == Capacity) return false;
        ::new (static_cast<void*>(slot(tail))) T(std::move(value));
        tail_.store(tail + 1, std::memory_order_release);  // publish the constructed item
        return true;
    }

    // Returns the oldest item, or nullopt when empty.
    std::optional<T> try_pop() {
        const std::size_t head = head_.load(std::memory_order_relaxed);  // only we write head
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        if (head == tail) return std::nullopt;
        T* item = slot(head);
        std::optional<T> out(std::move(*item));
        item->~T();
        head_.store(head + 1, std::memory_order_release);  // free the slot for the producer
        return out;
    }

    // Approximate count; exact only when called by the producer or consumer while the other is idle.
    // head is read before tail: head never passes the tail observed at any earlier moment, so the
    // difference cannot wrap.
    std::size_t size_approx() const {
        const std::size_t head = head_.load(std::memory_order_acquire);
        const std::size_t tail = tail_.load(std::memory_order_acquire);
        return tail - head;
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

private:
    T* slot(std::size_t index) noexcept {
        return std::launder(reinterpret_cast<T*>(storage_ + (index & (Capacity - 1)) * sizeof(T)));
    }

    // head_ and tail_ are monotonically increasing counters; the slot is index & (Capacity - 1).
    // Explicit padding keeps head_ and tail_ on separate 64-byte lines relative to the object
    // start (no alignas, which MSVC reports as C4324 padding).
    std::atomic<std::size_t> head_{0};
    std::byte pad0_[64 - sizeof(std::atomic<std::size_t>)];
    std::atomic<std::size_t> tail_{0};
    std::byte pad1_[64 - sizeof(std::atomic<std::size_t>)];
    alignas(T) std::byte storage_[sizeof(T) * Capacity];
};

}  // namespace cppref::concurrency
