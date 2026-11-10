#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

// Part 4 · Event-sourced bank account (engineer-owned).
//
// The event log is the source of truth. The state is a fold of the log:
// apply_event is the evolve step, decide_withdraw is the decide step for a
// command. Neither mutates its inputs.
namespace cppref::functional {

enum class AccountStatus { open, frozen };

struct Account {
    std::string account_id;
    std::string owner;
    std::int64_t balance_cents = 0;
    AccountStatus status = AccountStatus::open;

    friend bool operator==(const Account&, const Account&) = default;
};

struct Opened {
    std::string account_id;
    std::string owner;

    friend bool operator==(const Opened&, const Opened&) = default;
};

struct Deposited {
    std::int64_t amount_cents = 0;

    friend bool operator==(const Deposited&, const Deposited&) = default;
};

struct Withdrawn {
    std::int64_t amount_cents = 0;

    friend bool operator==(const Withdrawn&, const Withdrawn&) = default;
};

struct Frozen {
    friend bool operator==(const Frozen&, const Frozen&) = default;
};

struct Unfrozen {
    friend bool operator==(const Unfrozen&, const Unfrozen&) = default;
};

using AccountEvent = std::variant<Opened, Deposited, Withdrawn, Frozen, Unfrozen>;

enum class AccountError {
    already_opened,       // Opened applied to an existing account
    not_opened,           // any other event applied before Opened
    non_positive_amount,  // deposit or withdrawal amount <= 0
    account_frozen,       // money movement on a frozen account
    insufficient_funds,   // withdrawal above the balance
    empty_log,            // replay of zero events
};

/// Evolve step. `state` is std::nullopt before the account is opened.
/// Frozen is idempotent; Unfrozen restores AccountStatus::open.
/// Checks run in this order: opened/not opened, amount, frozen, funds.
[[nodiscard]] std::expected<Account, AccountError> apply_event(
    const std::optional<Account>& state, const AccountEvent& event);

/// Folds the whole log from "not opened". An empty log is empty_log.
[[nodiscard]] std::expected<Account, AccountError> replay(std::span<const AccountEvent> events);

/// State after each event. Stops at the first invalid event. An empty log
/// yields an empty vector. For a valid log, snapshots.back() == replay(events).
[[nodiscard]] std::expected<std::vector<Account>, AccountError> snapshots(
    std::span<const AccountEvent> events);

/// Command handler: validates a withdrawal against `account` and returns the
/// event to record. Uses the same rules and errors as apply_event.
[[nodiscard]] std::expected<Withdrawn, AccountError> decide_withdraw(const Account& account,
                                                                    std::int64_t amount_cents);

}  // namespace cppref::functional
