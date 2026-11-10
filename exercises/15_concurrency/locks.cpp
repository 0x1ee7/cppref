#include "locks.hpp"

#include <stdexcept>

namespace cppref::concurrency {

std::int64_t Counter::increment(std::string_view name, std::int64_t by) {
    (void)name;
    (void)by;
    throw std::logic_error("not implemented: Counter::increment");
}

std::int64_t Counter::get(std::string_view name) const {
    (void)name;
    throw std::logic_error("not implemented: Counter::get");
}

std::map<std::string, std::int64_t> Counter::snapshot() const {
    throw std::logic_error("not implemented: Counter::snapshot");
}

Account::Account(std::string id, std::int64_t balance) {
    (void)id;
    (void)balance;
    throw std::logic_error("not implemented: Account::Account");
}

const std::string& Account::id() const {
    throw std::logic_error("not implemented: Account::id");
}

std::int64_t Account::balance() const {
    throw std::logic_error("not implemented: Account::balance");
}

void Account::deposit(std::int64_t amount) {
    (void)amount;
    throw std::logic_error("not implemented: Account::deposit");
}

void Account::withdraw(std::int64_t amount) {
    (void)amount;
    throw std::logic_error("not implemented: Account::withdraw");
}

void transfer(Account& src, Account& dst, std::int64_t amount) {
    (void)src;
    (void)dst;
    (void)amount;
    throw std::logic_error("not implemented: transfer");
}

void RouteTable::replace(std::map<std::string, std::string> routes) {
    (void)routes;
    throw std::logic_error("not implemented: RouteTable::replace");
}

std::optional<std::string> RouteTable::lookup(std::string_view key) const {
    (void)key;
    throw std::logic_error("not implemented: RouteTable::lookup");
}

std::size_t RouteTable::size() const {
    throw std::logic_error("not implemented: RouteTable::size");
}

}  // namespace cppref::concurrency
