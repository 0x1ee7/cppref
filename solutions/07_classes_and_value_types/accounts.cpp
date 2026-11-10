#include "accounts.hpp"

#include <stdexcept>
#include <utility>

namespace cppref::classes {
namespace {

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && is_space(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_space(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

Money checked_overdraft(const Money& balance, const std::optional<Money>& overdraft) {
    if (!overdraft) {
        return Money::zero(balance.currency());
    }
    if (overdraft->currency() != balance.currency()) {
        throw std::invalid_argument("overdraft limit currency differs from the balance currency");
    }
    if (overdraft->is_negative()) {
        throw std::invalid_argument("overdraft limit must not be negative");
    }
    return *overdraft;
}

}  // namespace

// ---------------------------------------------------------------- Account

Account::Account(std::string owner, Money balance) : owner_(trim(owner)), balance_(std::move(balance)) {
    if (owner_.empty()) {
        throw std::invalid_argument("account owner must not be blank");
    }
}

std::string_view Account::owner() const {
    return owner_;
}

const Money& Account::balance() const {
    return balance_;
}

Money Account::available_funds() const {
    return balance_;
}

std::expected<Money, AccountError> Account::deposit(const Money& amount) {
    if (!amount.is_positive()) {
        return std::unexpected(AccountError::invalid_amount);
    }
    if (amount.currency() != balance_.currency()) {
        return std::unexpected(AccountError::currency_mismatch);
    }
    balance_ = balance_ + amount;
    return balance_;
}

std::expected<Money, AccountError> Account::withdraw(const Money& amount) {
    if (auto allowed = check_withdrawal(amount); !allowed) {
        return std::unexpected(allowed.error());
    }
    if (!amount.is_positive()) {
        return std::unexpected(AccountError::invalid_amount);
    }
    if (amount.currency() != balance_.currency()) {
        return std::unexpected(AccountError::currency_mismatch);
    }
    if (amount > available_funds()) {
        return std::unexpected(AccountError::insufficient_funds);
    }
    balance_ = balance_ - amount;
    on_withdrawn();
    return balance_;
}

std::expected<void, AccountError> Account::check_withdrawal(const Money&) const {
    return {};
}

void Account::on_withdrawn() {}

// ---------------------------------------------------------------- CheckingAccount

CheckingAccount::CheckingAccount(std::string owner, Money balance, std::optional<Money> overdraft_limit)
    : Account(std::move(owner), balance), overdraft_limit_(checked_overdraft(balance, overdraft_limit)) {}

const Money& CheckingAccount::overdraft_limit() const {
    return overdraft_limit_;
}

Money CheckingAccount::available_funds() const {
    return balance() + overdraft_limit_;
}

// ---------------------------------------------------------------- SavingsAccount

SavingsAccount::SavingsAccount(std::string owner, Money balance, int max_withdrawals)
    : Account(std::move(owner), std::move(balance)), max_withdrawals_(max_withdrawals) {
    if (max_withdrawals < 0) {
        throw std::invalid_argument("max_withdrawals must not be negative");
    }
}

int SavingsAccount::max_withdrawals() const {
    return max_withdrawals_;
}

int SavingsAccount::withdrawals_this_period() const {
    return withdrawals_this_period_;
}

void SavingsAccount::reset_period() {
    withdrawals_this_period_ = 0;
}

std::expected<void, AccountError> SavingsAccount::check_withdrawal(const Money&) const {
    if (withdrawals_this_period_ >= max_withdrawals_) {
        return std::unexpected(AccountError::withdrawal_limit_exceeded);
    }
    return {};
}

void SavingsAccount::on_withdrawn() {
    ++withdrawals_this_period_;
}

}  // namespace cppref::classes
