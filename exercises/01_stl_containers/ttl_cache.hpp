#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <vector>

#include "clock.hpp"

namespace cppref::stl {

/// Bounded gateway-response cache: least-recently-used eviction plus a per-entry
/// TTL. Expired entries never count toward capacity.
///
/// Semantics (the tests are the spec):
///  - An entry written at `t` expires at `t + ttl`; it is expired once `now >= expires_at`.
///  - `get`: a live entry is a hit and becomes most-recently used. A missing or
///    expired key is a miss; an expired entry is removed. Reading never extends TTL.
///  - `put`: insert or replace. Replacing refreshes both TTL and recency. Expired
///    entries are purged first, then LRU live entries are evicted until size <= max.
///  - `contains` / `keys` / `size` never change recency or the hit/miss counters.
///  - `keys()` is ordered least-recently-used first.
template <typename K, typename V, typename Hash = std::hash<K>>
class TtlCache {
public:
    /// @throws std::invalid_argument if `max_size == 0` or `ttl <= 0`.
    TtlCache(std::size_t max_size, std::chrono::milliseconds ttl, MonotonicClock clock = steady_clock_source()) {
        (void)max_size;
        (void)ttl;
        (void)clock;
        throw std::logic_error("not implemented: TtlCache::TtlCache");
    }

    /// Copy of the live value for `key`, or `std::nullopt`. Counts a hit or a miss.
    std::optional<V> get(const K& key) {
        (void)key;
        throw std::logic_error("not implemented: TtlCache::get");
    }

    /// Inserts or replaces `key`. Takes both arguments by value and moves them in.
    void put(K key, V value) {
        (void)key;
        (void)value;
        throw std::logic_error("not implemented: TtlCache::put");
    }

    /// True iff `key` is present and live. No side effects.
    bool contains(const K& key) const {
        (void)key;
        throw std::logic_error("not implemented: TtlCache::contains");
    }

    /// Live keys, least-recently-used first. No side effects.
    std::vector<K> keys() const {
        throw std::logic_error("not implemented: TtlCache::keys");
    }

    /// Number of live entries (expired ones excluded).
    std::size_t size() const {
        throw std::logic_error("not implemented: TtlCache::size");
    }

    /// Number of `get` calls that returned a value.
    std::size_t hits() const {
        throw std::logic_error("not implemented: TtlCache::hits");
    }

    /// Number of `get` calls that returned `std::nullopt`.
    std::size_t misses() const {
        throw std::logic_error("not implemented: TtlCache::misses");
    }

private:
    // Engineer: LRU list (front = least recent, back = most recent) of
    // {key, value, expires_at}; an index from K to list iterator (Hash);
    // the clock, ttl, capacity and the hit/miss counters.
};

}  // namespace cppref::stl
