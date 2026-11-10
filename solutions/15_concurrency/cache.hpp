#pragma once

#include <cstddef>
#include <functional>
#include <future>
#include <list>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace cppref::concurrency {

namespace detail {

// Not thread-safe by itself: callers hold the owning cache's mutex.
template <typename K, typename V>
class LruMap {
public:
    explicit LruMap(std::size_t capacity) : capacity_(capacity) {}

    // Marks the key most recently used on a hit.
    std::optional<V> find(const K& key) {
        auto it = index_.find(key);
        if (it == index_.end()) return std::nullopt;
        order_.splice(order_.begin(), order_, it->second);
        return it->second->value;
    }

    bool contains(const K& key) const { return index_.find(key) != index_.end(); }

    void put(const K& key, V value) {
        if (auto it = index_.find(key); it != index_.end()) {
            it->second->value = std::move(value);
            order_.splice(order_.begin(), order_, it->second);
            return;
        }
        if (order_.size() == capacity_) {
            index_.erase(order_.back().key);
            order_.pop_back();
        }
        order_.push_front(Entry{key, std::move(value)});
        index_.emplace(key, order_.begin());
    }

    std::size_t size() const { return order_.size(); }

private:
    struct Entry {
        K key;
        V value;
    };
    std::size_t capacity_;
    std::list<Entry> order_;  // front = most recently used
    std::unordered_map<K, typename std::list<Entry>::iterator> index_;
};

}  // namespace detail

// A thread-safe LRU cache with a fixed capacity. Eviction removes the least recently used entry.
// get() and put() both count as a use; contains() does not. K must be hashable.
template <typename K, typename V>
class BoundedCache {
public:
    // Throws std::invalid_argument if capacity == 0.
    explicit BoundedCache(std::size_t capacity) : lru_(validated(capacity)) {}

    BoundedCache(const BoundedCache&) = delete;
    BoundedCache& operator=(const BoundedCache&) = delete;

    // Returns a copy of the value and marks the key most recently used.
    std::optional<V> get(const K& key) {
        std::lock_guard lock(mutex_);
        auto hit = lru_.find(key);
        if (hit) {
            ++hits_;
        } else {
            ++misses_;
        }
        return hit;
    }

    // Inserts or updates the value; may evict the least recently used entry.
    void put(const K& key, V value) {
        std::lock_guard lock(mutex_);
        lru_.put(key, std::move(value));
    }

    // Membership test with no side effects on recency or the hit/miss counters.
    bool contains(const K& key) const {
        std::lock_guard lock(mutex_);
        return lru_.contains(key);
    }

    std::size_t size() const {
        std::lock_guard lock(mutex_);
        return lru_.size();
    }

    // Counts successful and failed get() calls.
    std::size_t hits() const {
        std::lock_guard lock(mutex_);
        return hits_;
    }

    std::size_t misses() const {
        std::lock_guard lock(mutex_);
        return misses_;
    }

private:
    static std::size_t validated(std::size_t capacity) {
        if (capacity == 0) throw std::invalid_argument("BoundedCache: capacity must be >= 1");
        return capacity;
    }

    mutable std::mutex mutex_;
    detail::LruMap<K, V> lru_;
    std::size_t hits_ = 0;
    std::size_t misses_ = 0;
};

// Bounded LRU cache that loads missing values with single-flight semantics: concurrent misses on
// the same key share one loader call. Loads run without holding the cache lock, so a slow key
// does not block other keys.
template <typename K, typename V>
class SingleFlightCache {
public:
    // Throws std::invalid_argument if capacity == 0.
    explicit SingleFlightCache(std::size_t capacity = 128) : lru_(validated(capacity)) {}

    SingleFlightCache(const SingleFlightCache&) = delete;
    SingleFlightCache& operator=(const SingleFlightCache&) = delete;

    // Returns the cached value, or runs loader(key) once and shares its result with every
    // concurrent caller for the same key. If the loader throws, every waiter receives that
    // exception and nothing is cached, so the next call retries the load.
    V get_or_load(const K& key, const std::function<V(const K&)>& loader) {
        std::optional<std::promise<V>> owned;  // engaged only for the caller that runs the load
        std::shared_future<V> pending;
        {
            std::lock_guard lock(mutex_);
            if (auto hit = lru_.find(key)) return *hit;
            if (auto it = inflight_.find(key); it != inflight_.end()) {
                pending = it->second;
            } else {
                owned.emplace();
                pending = owned->get_future().share();
                inflight_.emplace(key, pending);
                ++load_count_;
            }
        }

        if (!owned) return pending.get();  // rethrows the loader's exception, if any

        try {
            V value = loader(key);
            {
                std::lock_guard lock(mutex_);
                inflight_.erase(key);
                lru_.put(key, value);
            }
            owned->set_value(value);
            return value;
        } catch (...) {
            {
                std::lock_guard lock(mutex_);
                inflight_.erase(key);  // failures are not cached
            }
            owned->set_exception(std::current_exception());
            throw;
        }
    }

    // Number of loader invocations that have started since construction.
    std::size_t load_count() const {
        std::lock_guard lock(mutex_);
        return load_count_;
    }

private:
    static std::size_t validated(std::size_t capacity) {
        if (capacity == 0) throw std::invalid_argument("SingleFlightCache: capacity must be >= 1");
        return capacity;
    }

    mutable std::mutex mutex_;
    detail::LruMap<K, V> lru_;
    std::unordered_map<K, std::shared_future<V>> inflight_;
    std::size_t load_count_ = 0;
};

}  // namespace cppref::concurrency
