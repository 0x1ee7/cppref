// Part 4: Account hierarchy — inheritance with protected virtual hooks (non-virtual interface).
#pragma once

#include <expected>
#include <optional>
#include <string>
#include <string_view>

#include "money.hpp"

namespace cppref::classes {

enum class AccountError {
    invalid_amount,            ///< deposit/withdraw amount is zero or negative
    currency_mismatch,         ///< amount currency differs from the account currency
    insufficient_funds,        ///< amount exceeds available_funds(); balance unchanged
    withdrawal_limit_exceeded, ///< SavingsAccount period limit reached
};

/// Base account. deposit()/withdraw() are non-virtual; subclasses customise behaviour
/// only through the protected hooks. The copy operations are deleted so an Account
/// cannot be sliced; own accounts through pointers or unique_ptr.
class Account {
public:
    /// Throws std::invalid_argument on an empty or blank owner. Stored trimmed.
    Account(std::string owner, Money balance);
    Account(const Account&) = delete;
    Account& operator=(const Account&) = delete;
    virtual ~Account() = default;

    [[nodiscard]] std::string_view owner() const;
    [[nodiscard]] const Money& balance() const;

    /// What withdraw() may take. Base: the balance.
    [[nodiscard]] virtual Money available_funds() const;

    /// Returns the new balance. Errors: invalid_amount, currency_mismatch.
    [[nodiscard]] std::expected<Money, AccountError> deposit(const Money& amount);

    /// Order of checks: withdrawal_limit (hook), amount > 0, currency, funds.
    /// Returns the new balance. Errors: withdrawal_limit_exceeded, invalid_amount,
    /// currency_mismatch, insufficient_funds (balance unchanged).
    [[nodiscard]] std::expected<Money, AccountError> withdraw(const Money& amount);

protected:
    /// Called first by withdraw(). Base accepts everything.
    [[nodiscard]] virtual std::expected<void, AccountError> check_withdrawal(const Money& amount) const;

    /// Called after a successful withdrawal. Base does nothing.
    virtual void on_withdrawn();

    // engineer: private data members (owner_, balance_)
};

/// Balance may go negative down to -overdraft_limit.
class CheckingAccount : public Account {
public:
    /// overdraft_limit defaults to zero in the balance's currency. Throws std::invalid_argument
    /// if the limit is negative or in another currency.
    CheckingAccount(std::string owner, Money balance, std::optional<Money> overdraft_limit = std::nullopt);

    [[nodiscard]] const Money& overdraft_limit() const;

    /// balance + overdraft_limit.
    [[nodiscard]] Money available_funds() const override;

    // engineer: private data member (overdraft_limit_)
};

/// At most `max_withdrawals` successful withdrawals per period. Deposits are not counted.
class SavingsAccount : public Account {
public:
    /// Throws std::invalid_argument if max_withdrawals is negative.
    SavingsAccount(std::string owner, Money balance, int max_withdrawals = 3);

    [[nodiscard]] int max_withdrawals() const;
    [[nodiscard]] int withdrawals_this_period() const;
    void reset_period();

protected:
    /// Rejects with withdrawal_limit_exceeded before any amount validation.
    [[nodiscard]] std::expected<void, AccountError> check_withdrawal(const Money& amount) const override;

    /// Counts the successful withdrawal.
    void on_withdrawn() override;

    // engineer: private data members (max_withdrawals_, withdrawals_this_period_)
};

}  // namespace cppref::classes
