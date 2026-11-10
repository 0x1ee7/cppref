#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>

namespace cppref::concurrency {

// A thread-safe LRU cache with a fixed capacity. Eviction removes the least recently used entry.
// get() and put() both count as a use; contains() does not.
template <typename K, typename V>
class BoundedCache {
public:
    // Throws std::invalid_argument if capacity == 0.
    explicit BoundedCache(std::size_t capacity) {
        (void)capacity;
        throw std::logic_error("not implemented: BoundedCache::BoundedCache");
    }

    BoundedCache(const BoundedCache&) = delete;
    BoundedCache& operator=(const BoundedCache&) = delete;

    // Returns a copy of the value and marks the key most recently used.
    std::optional<V> get(const K& key) {
        (void)key;
        throw std::logic_error("not implemented: BoundedCache::get");
    }

    // Inserts or updates the value; may evict the least recently used entry.
    void put(const K& key, V value) {
        (void)key;
        (void)value;
        throw std::logic_error("not implemented: BoundedCache::put");
    }

    // Membership test with no side effects on recency or the hit/miss counters.
    bool contains(const K& key) const {
        (void)key;
        throw std::logic_error("not implemented: BoundedCache::contains");
    }

    std::size_t size() const {
        throw std::logic_error("not implemented: BoundedCache::size");
    }

    // Counts successful and failed get() calls.
    std::size_t hits() const {
        throw std::logic_error("not implemented: BoundedCache::hits");
    }

    std::size_t misses() const {
        throw std::logic_error("not implemented: BoundedCache::misses");
    }

private:
    // Private members for the engineer to add: capacity, list + index for LRU, a mutex, counters.
};

// Bounded LRU cache that loads missing values with single-flight semantics: concurrent misses on
// the same key share one loader call. Loads run without holding the cache lock, so a slow key
// does not block other keys.
template <typename K, typename V>
class SingleFlightCache {
public:
    // Throws std::invalid_argument if capacity == 0.
    explicit SingleFlightCache(std::size_t capacity = 128) {
        (void)capacity;
        throw std::logic_error("not implemented: SingleFlightCache::SingleFlightCache");
    }

    SingleFlightCache(const SingleFlightCache&) = delete;
    SingleFlightCache& operator=(const SingleFlightCache&) = delete;

    // Returns the cached value, or runs loader(key) once and shares its result with every
    // concurrent caller for the same key. If the loader throws, every waiter receives that
    // exception and nothing is cached, so the next call retries the load.
    V get_or_load(const K& key, const std::function<V(const K&)>& loader) {
        (void)key;
        (void)loader;
        throw std::logic_error("not implemented: SingleFlightCache::get_or_load");
    }

    // Number of loader invocations that have started since construction.
    std::size_t load_count() const {
        throw std::logic_error("not implemented: SingleFlightCache::load_count");
    }

private:
    // Private members for the engineer to add: LRU state, in-flight map of shared_future, mutex.
};

}  // namespace cppref::concurrency
