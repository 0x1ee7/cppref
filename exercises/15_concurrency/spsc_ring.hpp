#pragma once

#include <cstddef>
#include <new>
#include <optional>
#include <stdexcept>
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
        // Private members for the engineer: destroy any values between tail and head.
    }

    // Returns false (leaving `value` untouched) when the ring is full.
    bool try_push(T&& value) {
        (void)value;
        throw std::logic_error("not implemented: SpscRing::try_push");
    }

    // Returns the oldest item, or nullopt when empty.
    std::optional<T> try_pop() {
        throw std::logic_error("not implemented: SpscRing::try_pop");
    }

    // Approximate count; exact only when called by the producer or consumer while the other is idle.
    std::size_t size_approx() const {
        throw std::logic_error("not implemented: SpscRing::size_approx");
    }

    static constexpr std::size_t capacity() noexcept { return Capacity; }

private:
    // Private members for the engineer to add: aligned head/tail atomics and raw storage for T.
    // Use std::hardware_destructive_interference_size only if guarded; otherwise use a fixed 64.
};

}  // namespace cppref::concurrency
