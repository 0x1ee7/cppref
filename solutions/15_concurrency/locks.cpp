#include "locks.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace cppref::concurrency {

std::int64_t Counter::increment(std::string_view name, std::int64_t by) {
    if (by < 0) throw std::invalid_argument("Counter::increment: by must be >= 0");
    std::lock_guard lock(mutex_);
    auto it = counts_.find(name);
    if (it == counts_.end()) it = counts_.emplace(std::string(name), 0).first;
    it->second += by;
    return it->second;
}

std::int64_t Counter::get(std::string_view name) const {
    std::lock_guard lock(mutex_);
    auto it = counts_.find(name);
    return it == counts_.end() ? 0 : it->second;
}

std::map<std::string, std::int64_t> Counter::snapshot() const {
    std::lock_guard lock(mutex_);
    return {counts_.begin(), counts_.end()};
}

Account::Account(std::string id, std::int64_t balance) : id_(std::move(id)), balance_(balance) {
    if (id_.empty()) throw std::invalid_argument("Account: id must not be empty");
    if (balance_ < 0) throw std::invalid_argument("Account: opening balance must be >= 0");
}

const std::string& Account::id() const {
    return id_;  // immutable after construction, no lock needed
}

std::int64_t Account::balance() const {
    std::lock_guard lock(mutex_);
    return balance_;
}

void Account::deposit(std::int64_t amount) {
    if (amount <= 0) throw std::invalid_argument("Account::deposit: amount must be > 0");
    std::lock_guard lock(mutex_);
    balance_ += amount;
}

void Account::withdraw(std::int64_t amount) {
    if (amount <= 0) throw std::invalid_argument("Account::withdraw: amount must be > 0");
    std::lock_guard lock(mutex_);
    if (amount > balance_) throw InsufficientFunds("withdraw: insufficient funds in " + id_);
    balance_ -= amount;
}

void transfer(Account& src, Account& dst, std::int64_t amount) {
    if (amount <= 0) throw std::invalid_argument("transfer: amount must be > 0");
    if (&src == &dst) throw std::invalid_argument("transfer: source and destination are the same");
    // scoped_lock acquires both mutexes with a deadlock-avoidance algorithm, whatever the order.
    std::scoped_lock lock(src.mutex_, dst.mutex_);
    if (src.balance_ < amount) throw InsufficientFunds("transfer: insufficient funds in " + src.id_);
    src.balance_ -= amount;
    dst.balance_ += amount;
}

void RouteTable::replace(std::map<std::string, std::string> routes) {
    // Build the new table outside the lock; swap it in under the writer lock.
    Routes next(routes.begin(), routes.end());
    std::unique_lock lock(mutex_);
    routes_.swap(next);
    // `next` now holds the old table and is destroyed after the lock is released.
}

std::optional<std::string> RouteTable::lookup(std::string_view key) const {
    std::shared_lock lock(mutex_);
    auto it = routes_.find(key);
    if (it == routes_.end()) return std::nullopt;
    return it->second;
}

std::size_t RouteTable::size() const {
    std::shared_lock lock(mutex_);
    return routes_.size();
}

}  // namespace cppref::concurrency
