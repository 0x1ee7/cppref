#pragma once

#include <chrono>
#include <cstddef>
#include <functional>
#include <list>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>
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
    TtlCache(std::size_t max_size, std::chrono::milliseconds ttl, MonotonicClock clock = steady_clock_source())
        : max_size_{max_size}, ttl_{ttl}, clock_{std::move(clock)} {
        if (max_size_ == 0) {
            throw std::invalid_argument("TtlCache: max_size must be positive");
        }
        if (ttl_ <= std::chrono::milliseconds::zero()) {
            throw std::invalid_argument("TtlCache: ttl must be positive");
        }
    }

    /// Copy of the live value for `key`, or `std::nullopt`. Counts a hit or a miss.
    std::optional<V> get(const K& key) {
        const Tp now = clock_();
        const auto it = index_.find(key);
        if (it == index_.end() || expired(*it->second, now)) {
            if (it != index_.end()) remove(it);
            ++misses_;
            return std::nullopt;
        }
        lru_.splice(lru_.end(), lru_, it->second);
        ++hits_;
        return it->second->value;
    }

    /// Inserts or replaces `key`. Takes both arguments by value and moves them in.
    void put(K key, V value) {
        const Tp now = clock_();
        purge_expired(now);
        const Tp expires_at = now + ttl_;

        if (const auto it = index_.find(key); it != index_.end()) {
            it->second->value = std::move(value);
            it->second->expires_at = expires_at;
            lru_.splice(lru_.end(), lru_, it->second);
            return;
        }

        lru_.push_back(Entry{std::move(key), std::move(value), expires_at});
        index_.emplace(lru_.back().key, std::prev(lru_.end()));

        while (lru_.size() > max_size_) {
            index_.erase(lru_.front().key);
            lru_.pop_front();
        }
    }

    /// True iff `key` is present and live. No side effects.
    bool contains(const K& key) const {
        const Tp now = clock_();
        const auto it = index_.find(key);
        return it != index_.end() && !expired(*it->second, now);
    }

    /// Live keys, least-recently-used first. No side effects.
    std::vector<K> keys() const {
        const Tp now = clock_();
        std::vector<K> out;
        out.reserve(index_.size());
        for (const Entry& e : lru_) {
            if (!expired(e, now)) out.push_back(e.key);
        }
        return out;
    }

    /// Number of live entries (expired ones excluded).
    std::size_t size() const {
        const Tp now = clock_();
        std::size_t live = 0;
        for (const Entry& e : lru_) {
            if (!expired(e, now)) ++live;
        }
        return live;
    }

    /// Number of `get` calls that returned a value.
    std::size_t hits() const { return hits_; }

    /// Number of `get` calls that returned `std::nullopt`.
    std::size_t misses() const { return misses_; }

private:
    using Tp = std::chrono::steady_clock::time_point;
    struct Entry {
        K key;
        V value;
        Tp expires_at;
    };
    using Lru = std::list<Entry>;
    using Index = std::unordered_map<K, typename Lru::iterator, Hash>;

    static bool expired(const Entry& e, Tp now) { return now >= e.expires_at; }

    void remove(typename Index::iterator it) {
        lru_.erase(it->second);
        index_.erase(it);
    }

    // O(n) scan; expiry is not ordered by recency (put refreshes TTL, get does not).
    void purge_expired(Tp now) {
        for (auto it = lru_.begin(); it != lru_.end();) {
            if (expired(*it, now)) {
                index_.erase(it->key);
                it = lru_.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::size_t max_size_;
    std::chrono::milliseconds ttl_;
    MonotonicClock clock_;
    Lru lru_;  // front = least recently used, back = most recently used
    Index index_;
    std::size_t hits_{0};
    std::size_t misses_{0};
};

}  // namespace cppref::stl
