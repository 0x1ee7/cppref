#include "accounts.hpp"

#include <stdexcept>
#include <string>
#include <utility>

namespace cppref::classes {
namespace {

[[noreturn]] void not_implemented(const char* name) {
    throw std::logic_error(std::string("not implemented: ") + name);
}

}  // namespace

Account::Account(std::string, Money) {
    not_implemented("Account::Account");
}

std::string_view Account::owner() const {
    not_implemented("Account::owner");
}

const Money& Account::balance() const {
    not_implemented("Account::balance");
}

Money Account::available_funds() const {
    not_implemented("Account::available_funds");
}

std::expected<Money, AccountError> Account::deposit(const Money&) {
    not_implemented("Account::deposit");
}

std::expected<Money, AccountError> Account::withdraw(const Money&) {
    not_implemented("Account::withdraw");
}

std::expected<void, AccountError> Account::check_withdrawal(const Money&) const {
    not_implemented("Account::check_withdrawal");
}

void Account::on_withdrawn() {
    not_implemented("Account::on_withdrawn");
}

CheckingAccount::CheckingAccount(std::string owner, Money balance, std::optional<Money>)
    : Account(std::move(owner), std::move(balance)) {
    not_implemented("CheckingAccount::CheckingAccount");
}

const Money& CheckingAccount::overdraft_limit() const {
    not_implemented("CheckingAccount::overdraft_limit");
}

Money CheckingAccount::available_funds() const {
    not_implemented("CheckingAccount::available_funds");
}

SavingsAccount::SavingsAccount(std::string owner, Money balance, int)
    : Account(std::move(owner), std::move(balance)) {
    not_implemented("SavingsAccount::SavingsAccount");
}

int SavingsAccount::max_withdrawals() const {
    not_implemented("SavingsAccount::max_withdrawals");
}

int SavingsAccount::withdrawals_this_period() const {
    not_implemented("SavingsAccount::withdrawals_this_period");
}

void SavingsAccount::reset_period() {
    not_implemented("SavingsAccount::reset_period");
}

std::expected<void, AccountError> SavingsAccount::check_withdrawal(const Money&) const {
    not_implemented("SavingsAccount::check_withdrawal");
}

void SavingsAccount::on_withdrawn() {
    not_implemented("SavingsAccount::on_withdrawn");
}

}  // namespace cppref::classes
