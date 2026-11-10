#include "account.hpp"

#include "variant_utils.hpp"

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace cppref::functional {

std::expected<Account, AccountError> apply_event(const std::optional<Account>& state,
                                                 const AccountEvent& event) {
    using Result = std::expected<Account, AccountError>;

    if (const auto* opened = std::get_if<Opened>(&event)) {
        if (state) {
            return std::unexpected(AccountError::already_opened);
        }
        return Account{opened->account_id, opened->owner, 0, AccountStatus::open};
    }
    if (!state) {
        return std::unexpected(AccountError::not_opened);
    }

    Account account = *state;
    return std::visit(
        overloaded{
            [&account](const Deposited& dep) -> Result {
                if (dep.amount_cents <= 0) {
                    return std::unexpected(AccountError::non_positive_amount);
                }
                if (account.status == AccountStatus::frozen) {
                    return std::unexpected(AccountError::account_frozen);
                }
                account.balance_cents += dep.amount_cents;
                return account;
            },
            [&account](const Withdrawn& wd) -> Result {
                if (wd.amount_cents <= 0) {
                    return std::unexpected(AccountError::non_positive_amount);
                }
                if (account.status == AccountStatus::frozen) {
                    return std::unexpected(AccountError::account_frozen);
                }
                if (account.balance_cents < wd.amount_cents) {
                    return std::unexpected(AccountError::insufficient_funds);
                }
                account.balance_cents -= wd.amount_cents;
                return account;
            },
            [&account](const Frozen&) -> Result {
                account.status = AccountStatus::frozen;
                return account;
            },
            [&account](const Unfrozen&) -> Result {
                account.status = AccountStatus::open;
                return account;
            },
            [](const Opened&) -> Result {
                // Handled above; an open event never reaches this visitor arm.
                return std::unexpected(AccountError::already_opened);
            },
        },
        event);
}

std::expected<Account, AccountError> replay(std::span<const AccountEvent> events) {
    if (events.empty()) {
        return std::unexpected(AccountError::empty_log);
    }
    std::optional<Account> state;
    for (const AccountEvent& event : events) {
        auto next = apply_event(state, event);
        if (!next) {
            return std::unexpected(next.error());
        }
        state = std::move(*next);
    }
    return *state;
}

std::expected<std::vector<Account>, AccountError> snapshots(std::span<const AccountEvent> events) {
    std::vector<Account> out;
    out.reserve(events.size());
    std::optional<Account> state;
    for (const AccountEvent& event : events) {
        auto next = apply_event(state, event);
        if (!next) {
            return std::unexpected(next.error());
        }
        state = std::move(*next);
        out.push_back(*state);
    }
    return out;
}

std::expected<Withdrawn, AccountError> decide_withdraw(const Account& account, std::int64_t amount_cents) {
    if (amount_cents <= 0) {
        return std::unexpected(AccountError::non_positive_amount);
    }
    if (account.status == AccountStatus::frozen) {
        return std::unexpected(AccountError::account_frozen);
    }
    if (account.balance_cents < amount_cents) {
        return std::unexpected(AccountError::insufficient_funds);
    }
    return Withdrawn{amount_cents};
}

}  // namespace cppref::functional
