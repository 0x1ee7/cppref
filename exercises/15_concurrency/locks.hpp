#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cppref::concurrency {

// Named counters shared across threads. Every operation is atomic with respect to the others.
class Counter {
public:
    // Adds `by` (must be >= 0; negative values throw std::invalid_argument) and returns the new value.
    // Unknown names start at 0.
    std::int64_t increment(std::string_view name, std::int64_t by = 1);

    // Current value; 0 for unknown names.
    std::int64_t get(std::string_view name) const;

    // Consistent copy of every counter.
    std::map<std::string, std::int64_t> snapshot() const;

private:
    // Private members for the engineer to add: a mutable mutex and the name -> value map.
};

// Thrown by Account::withdraw and transfer when the balance is too small.
class InsufficientFunds : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// A bank account. Each account owns a mutex; use transfer() to move money between accounts.
// Not copyable: an account is an identity, and copies would silently fork the balance.
class Account {
public:
    // Throws std::invalid_argument for an empty id or a negative opening balance.
    explicit Account(std::string id, std::int64_t balance = 0);

    Account(const Account&) = delete;
    Account& operator=(const Account&) = delete;

    const std::string& id() const;
    std::int64_t balance() const;

    // Throws std::invalid_argument if amount <= 0.
    void deposit(std::int64_t amount);

    // Throws std::invalid_argument if amount <= 0, InsufficientFunds if amount > balance.
    void withdraw(std::int64_t amount);

    // Moves `amount` from src to dst atomically: both account locks are held together.
    friend void transfer(Account& src, Account& dst, std::int64_t amount);

private:
    // Private members for the engineer to add: id, balance, and a mutex.
};

// Moves `amount` from src to dst. Throws std::invalid_argument if amount <= 0 or src and dst
// are the same account; InsufficientFunds (with no change to either balance) if src is short.
void transfer(Account& src, Account& dst, std::int64_t amount);

// A routing table that many threads read and that is rarely replaced wholesale.
class RouteTable {
public:
    // Atomically swaps in a new table. Readers see either the old table or the new one, never a mix.
    void replace(std::map<std::string, std::string> routes);

    // Exact-key lookup; nullopt when the key is absent.
    std::optional<std::string> lookup(std::string_view key) const;

    std::size_t size() const;

private:
    // Private members for the engineer to add: the routes map and a shared_mutex.
};

}  // namespace cppref::concurrency
