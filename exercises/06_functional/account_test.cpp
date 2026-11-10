#include "account.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace fn = cppref::functional;

namespace {

fn::Account open_account(std::int64_t balance, fn::AccountStatus status = fn::AccountStatus::open) {
    return fn::Account{"acc-1", "alice", balance, status};
}

std::vector<fn::AccountEvent> standard_log() {
    return {fn::Opened{"acc-1", "alice"}, fn::Deposited{1000}, fn::Withdrawn{300}};
}

}  // namespace

TEST(Part4EventSourcedAccount, ReplayFoldsDepositAndWithdraw) {
    auto log = standard_log();
    auto acc = fn::replay(log);
    ASSERT_TRUE(acc.has_value());
    EXPECT_EQ(*acc, (fn::Account{"acc-1", "alice", 700, fn::AccountStatus::open}));
}

TEST(Part4EventSourcedAccount, OpenedRulesOnTheFirstEvent) {
    auto existing = open_account(0);
    auto twice = fn::apply_event(existing, fn::AccountEvent{fn::Opened{"acc-1", "alice"}});
    ASSERT_FALSE(twice.has_value());
    EXPECT_EQ(twice.error(), fn::AccountError::already_opened);

    auto early = fn::apply_event(std::nullopt, fn::AccountEvent{fn::Deposited{5}});
    ASSERT_FALSE(early.has_value());
    EXPECT_EQ(early.error(), fn::AccountError::not_opened);
}

TEST(Part4EventSourcedAccount, ReplayOfEmptyLogIsAnError) {
    std::vector<fn::AccountEvent> none;
    auto r = fn::replay(none);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), fn::AccountError::empty_log);
}

TEST(Part4EventSourcedAccount, NonPositiveAmountsAreRejected) {
    auto acc = open_account(100);
    auto deposit = fn::apply_event(acc, fn::AccountEvent{fn::Deposited{0}});
    ASSERT_FALSE(deposit.has_value());
    EXPECT_EQ(deposit.error(), fn::AccountError::non_positive_amount);

    auto withdraw = fn::apply_event(acc, fn::AccountEvent{fn::Withdrawn{-5}});
    ASSERT_FALSE(withdraw.has_value());
    EXPECT_EQ(withdraw.error(), fn::AccountError::non_positive_amount);
}

TEST(Part4EventSourcedAccount, FrozenAccountRejectsMoneyMovement) {
    auto frozen = open_account(100, fn::AccountStatus::frozen);
    auto deposit = fn::apply_event(frozen, fn::AccountEvent{fn::Deposited{10}});
    ASSERT_FALSE(deposit.has_value());
    EXPECT_EQ(deposit.error(), fn::AccountError::account_frozen);

    auto withdraw = fn::apply_event(frozen, fn::AccountEvent{fn::Withdrawn{10}});
    ASSERT_FALSE(withdraw.has_value());
    EXPECT_EQ(withdraw.error(), fn::AccountError::account_frozen);
}

TEST(Part4EventSourcedAccount, FreezeIsIdempotentAndUnfreezeRestores) {
    std::vector<fn::AccountEvent> log{fn::Opened{"acc-1", "alice"}, fn::Frozen{}, fn::Frozen{}};
    auto frozen = fn::replay(log);
    ASSERT_TRUE(frozen.has_value());
    EXPECT_EQ(frozen->status, fn::AccountStatus::frozen);

    log.emplace_back(fn::Unfrozen{});
    auto thawed = fn::replay(log);
    ASSERT_TRUE(thawed.has_value());
    EXPECT_EQ(thawed->status, fn::AccountStatus::open);
}

TEST(Part4EventSourcedAccount, WithdrawalMayNotExceedBalance) {
    auto acc = open_account(100);
    auto over = fn::apply_event(acc, fn::AccountEvent{fn::Withdrawn{101}});
    ASSERT_FALSE(over.has_value());
    EXPECT_EQ(over.error(), fn::AccountError::insufficient_funds);

    auto exact = fn::apply_event(acc, fn::AccountEvent{fn::Withdrawn{100}});
    ASSERT_TRUE(exact.has_value());
    EXPECT_EQ(exact->balance_cents, 0);
}

TEST(Part4EventSourcedAccount, SnapshotsRecordEveryState) {
    auto log = standard_log();
    auto snaps = fn::snapshots(log);
    ASSERT_TRUE(snaps.has_value());
    ASSERT_EQ(snaps->size(), 3u);
    EXPECT_EQ((*snaps)[0].balance_cents, 0);
    EXPECT_EQ((*snaps)[1].balance_cents, 1000);
    EXPECT_EQ((*snaps)[2].balance_cents, 700);
    EXPECT_EQ(snaps->back(), fn::replay(log).value());
}

TEST(Part4EventSourcedAccount, SnapshotsOfEmptyLogIsEmptyAndInvalidLogFails) {
    std::vector<fn::AccountEvent> none;
    auto empty = fn::snapshots(none);
    ASSERT_TRUE(empty.has_value());
    EXPECT_TRUE(empty->empty());

    std::vector<fn::AccountEvent> bad{fn::Deposited{5}};
    auto invalid = fn::snapshots(bad);
    ASSERT_FALSE(invalid.has_value());
    EXPECT_EQ(invalid.error(), fn::AccountError::not_opened);
}

TEST(Part4EventSourcedAccount, DecideWithdrawChecksBeforeRecording) {
    auto acc = open_account(300);

    auto ok = fn::decide_withdraw(acc, 200);
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(ok->amount_cents, 200);

    auto broke = fn::decide_withdraw(acc, 500);
    ASSERT_FALSE(broke.has_value());
    EXPECT_EQ(broke.error(), fn::AccountError::insufficient_funds);

    auto zero = fn::decide_withdraw(acc, 0);
    ASSERT_FALSE(zero.has_value());
    EXPECT_EQ(zero.error(), fn::AccountError::non_positive_amount);

    auto frozen = fn::decide_withdraw(open_account(300, fn::AccountStatus::frozen), 1);
    ASSERT_FALSE(frozen.has_value());
    EXPECT_EQ(frozen.error(), fn::AccountError::account_frozen);
}
