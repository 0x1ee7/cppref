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
/// only through the protected hooks. Copies are deleted so an Account cannot be sliced.
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

    [[nodiscard]] std::expected<Money, AccountError> deposit(const Money& amount);
    [[nodiscard]] std::expected<Money, AccountError> withdraw(const Money& amount);

protected:
    /// Called first by withdraw(). Base accepts everything.
    [[nodiscard]] virtual std::expected<void, AccountError> check_withdrawal(const Money& amount) const;

    /// Called after a successful withdrawal. Base does nothing.
    virtual void on_withdrawn();

private:
    std::string owner_;
    Money balance_;
};

/// Balance may go negative down to -overdraft_limit.
class CheckingAccount : public Account {
public:
    CheckingAccount(std::string owner, Money balance, std::optional<Money> overdraft_limit = std::nullopt);

    [[nodiscard]] const Money& overdraft_limit() const;

    /// balance + overdraft_limit.
    [[nodiscard]] Money available_funds() const override;

private:
    Money overdraft_limit_;
};

/// At most `max_withdrawals` successful withdrawals per period. Deposits are not counted.
class SavingsAccount : public Account {
public:
    SavingsAccount(std::string owner, Money balance, int max_withdrawals = 3);

    [[nodiscard]] int max_withdrawals() const;
    [[nodiscard]] int withdrawals_this_period() const;
    void reset_period();

protected:
    [[nodiscard]] std::expected<void, AccountError> check_withdrawal(const Money& amount) const override;
    void on_withdrawn() override;

private:
    int max_withdrawals_;
    int withdrawals_this_period_ = 0;
};

}  // namespace cppref::classes
