// Part 4: Account hierarchy — non-virtual interface with protected hooks.
#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>

#include "accounts.hpp"

namespace cc = cppref::classes;

namespace {

cc::Currency currency(std::string_view code) {
    return cc::Currency::parse(code).value();
}

cc::Money money(std::string_view text, std::string_view code = "USD") {
    return cc::Money::parse(text, currency(code)).value();
}

}  // namespace

TEST(Part4Account, DepositReturnsNewBalance) {
    cc::Account account("Ada", money("10.00"));
    auto after = account.deposit(money("5.00"));
    ASSERT_TRUE(after.has_value());
    EXPECT_EQ(*after, money("15.00"));
    EXPECT_EQ(account.balance(), money("15.00"));
}

TEST(Part4Account, DepositRejectsNonPositiveAndForeignCurrency) {
    cc::Account account("Ada", money("10.00"));
    ASSERT_FALSE(account.deposit(money("0")).has_value());
    EXPECT_EQ(account.deposit(money("0")).error(), cc::AccountError::invalid_amount);
    EXPECT_EQ(account.deposit(money("-1")).error(), cc::AccountError::invalid_amount);
    EXPECT_EQ(account.deposit(money("1", "EUR")).error(), cc::AccountError::currency_mismatch);
    EXPECT_EQ(account.balance(), money("10.00"));
}

TEST(Part4Account, WithdrawBoundedByAvailableFunds) {
    cc::Account account("Ada", money("10.00"));
    auto drained = account.withdraw(money("10.00"));
    ASSERT_TRUE(drained.has_value());
    EXPECT_TRUE(drained->is_zero());

    auto refused = account.withdraw(money("0.01"));
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error(), cc::AccountError::insufficient_funds);
    EXPECT_TRUE(account.balance().is_zero());
}

TEST(Part4Account, OwnerIsTrimmedAndRequired) {
    cc::Account account("  Ada  ", money("0"));
    EXPECT_EQ(account.owner(), "Ada");
    EXPECT_THROW(cc::Account("   ", money("0")), std::invalid_argument);
}

TEST(Part4Checking, OverdraftExtendsAvailableFunds) {
    cc::CheckingAccount checking("Bo", money("10.00"), money("50.00"));
    EXPECT_EQ(checking.available_funds(), money("60.00"));

    const cc::Account& as_base = checking;
    EXPECT_EQ(as_base.available_funds(), money("60.00"));

    ASSERT_TRUE(checking.withdraw(money("60.00")).has_value());
    EXPECT_EQ(checking.balance(), money("-50.00"));
    EXPECT_EQ(checking.withdraw(money("0.01")).error(), cc::AccountError::insufficient_funds);
}

TEST(Part4Checking, OverdraftDefaultsToZeroAndIsValidated) {
    cc::CheckingAccount plain("Cy", money("10.00"));
    EXPECT_TRUE(plain.overdraft_limit().is_zero());
    EXPECT_EQ(plain.withdraw(money("10.01")).error(), cc::AccountError::insufficient_funds);

    EXPECT_THROW(cc::CheckingAccount("Cy", money("10.00"), money("-1.00")), std::invalid_argument);
    EXPECT_THROW(cc::CheckingAccount("Cy", money("10.00"), money("5.00", "EUR")), std::invalid_argument);
}

TEST(Part4Savings, OnlyWithdrawalsAreCountedAgainstTheLimit) {
    cc::SavingsAccount savings("Di", money("100.00"), 2);
    ASSERT_TRUE(savings.deposit(money("10.00")).has_value());
    ASSERT_TRUE(savings.deposit(money("10.00")).has_value());
    ASSERT_TRUE(savings.withdraw(money("1.00")).has_value());
    ASSERT_TRUE(savings.withdraw(money("1.00")).has_value());

    auto third = savings.withdraw(money("1.00"));
    ASSERT_FALSE(third.has_value());
    EXPECT_EQ(third.error(), cc::AccountError::withdrawal_limit_exceeded);
    EXPECT_EQ(savings.withdrawals_this_period(), 2);
    EXPECT_EQ(savings.balance(), money("118.00"));
}

TEST(Part4Savings, LimitIsCheckedBeforeAmountValidation) {
    cc::SavingsAccount savings("Di", money("100.00"), 0);
    EXPECT_EQ(savings.withdraw(money("-5.00")).error(), cc::AccountError::withdrawal_limit_exceeded);
}

TEST(Part4Savings, FailedWithdrawalsDoNotConsumeTheAllowance) {
    cc::SavingsAccount savings("Di", money("5.00"), 1);
    EXPECT_EQ(savings.withdraw(money("500.00")).error(), cc::AccountError::insufficient_funds);
    EXPECT_EQ(savings.withdrawals_this_period(), 0);

    ASSERT_TRUE(savings.withdraw(money("1.00")).has_value());
    savings.reset_period();
    EXPECT_EQ(savings.withdrawals_this_period(), 0);
    EXPECT_TRUE(savings.withdraw(money("1.00")).has_value());
}

TEST(Part4Savings, NegativeLimitRejectedAndAccessorReports) {
    EXPECT_THROW(cc::SavingsAccount("Di", money("1.00"), -1), std::invalid_argument);
    cc::SavingsAccount savings("Di", money("1.00"), 4);
    EXPECT_EQ(savings.max_withdrawals(), 4);
}

TEST(Part4Polymorphism, WithdrawDispatchesThroughBaseReference) {
    std::unique_ptr<cc::Account> account = std::make_unique<cc::SavingsAccount>("Ed", money("10.00"), 1);
    ASSERT_TRUE(account->withdraw(money("1.00")).has_value());
    EXPECT_EQ(account->withdraw(money("1.00")).error(), cc::AccountError::withdrawal_limit_exceeded);
}
