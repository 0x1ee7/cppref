#include "account.hpp"

#include <stdexcept>

// Part 4 · Event-sourced account. Stub: implement every function.

namespace cppref::functional {

std::expected<Account, AccountError> apply_event(const std::optional<Account>&,
                                                 const AccountEvent&) {
    throw std::logic_error("not implemented: apply_event");
}

std::expected<Account, AccountError> replay(std::span<const AccountEvent>) {
    throw std::logic_error("not implemented: replay");
}

std::expected<std::vector<Account>, AccountError> snapshots(std::span<const AccountEvent>) {
    throw std::logic_error("not implemented: snapshots");
}

std::expected<Withdrawn, AccountError> decide_withdraw(const Account&, std::int64_t) {
    throw std::logic_error("not implemented: decide_withdraw");
}

}  // namespace cppref::functional
