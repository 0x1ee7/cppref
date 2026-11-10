#include "locks.hpp"
#include "test_support_test.hpp"

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

using cppref::concurrency::Account;
using cppref::concurrency::Counter;
using cppref::concurrency::InsufficientFunds;
using cppref::concurrency::RouteTable;
using cppref::concurrency::testing::Background;
using cppref::concurrency::testing::parallel_for;
using cppref::concurrency::transfer;

TEST(Part3Counter, ConcurrentIncrementsAreNotLost) {
    Counter counter;
    parallel_for(8, [&](std::size_t) {
        for (int i = 0; i < 1000; ++i) counter.increment("hits");
    });
    EXPECT_EQ(counter.get("hits"), 8000);
}

TEST(Part3Counter, IncrementReturnsNewValue) {
    Counter counter;
    EXPECT_EQ(counter.get("missing"), 0);
    EXPECT_EQ(counter.increment("x"), 1);
    EXPECT_EQ(counter.increment("x", 4), 5);
    EXPECT_EQ(counter.get("x"), 5);
}

TEST(Part3Counter, NegativeIncrementRejected) {
    Counter counter;
    EXPECT_THROW(counter.increment("x", -1), std::invalid_argument);
    EXPECT_EQ(counter.get("x"), 0);
}

TEST(Part3Counter, SnapshotIsAStableCopy) {
    Counter counter;
    counter.increment("a");
    counter.increment("b", 2);

    const auto snap = counter.snapshot();
    EXPECT_EQ(snap, (std::map<std::string, std::int64_t>{{"a", 1}, {"b", 2}}));

    counter.increment("a");
    EXPECT_EQ(snap.at("a"), 1);
}

TEST(Part3Accounts, TransferMovesMoney) {
    Account a("alice", 100);
    Account b("bob", 0);
    transfer(a, b, 30);
    EXPECT_EQ(a.balance(), 70);
    EXPECT_EQ(b.balance(), 30);
    EXPECT_EQ(a.id(), "alice");
}

TEST(Part3Accounts, OverdraftRejectedWithoutChange) {
    Account a("alice", 10);
    Account b("bob", 0);
    EXPECT_THROW(transfer(a, b, 20), InsufficientFunds);
    EXPECT_EQ(a.balance(), 10);
    EXPECT_EQ(b.balance(), 0);
}

TEST(Part3Accounts, InvalidArgumentsRejected) {
    EXPECT_THROW(Account("", 0), std::invalid_argument);
    EXPECT_THROW(Account("neg", -1), std::invalid_argument);

    Account a("a", 50);
    Account b("b", 0);
    EXPECT_THROW(a.deposit(0), std::invalid_argument);
    EXPECT_THROW(a.withdraw(-5), std::invalid_argument);
    EXPECT_THROW(transfer(a, a, 1), std::invalid_argument);
    EXPECT_THROW(transfer(a, b, 0), std::invalid_argument);
    EXPECT_EQ(a.balance(), 50);
}

TEST(Part3Accounts, OpposingTransfersDoNotDeadlock) {
    // Each account can send at most 2000 in total, so 2000 of opening balance can never overdraw.
    Account a("a", 2000);
    Account b("b", 2000);
    parallel_for(2, [&](std::size_t lane) {
        for (int i = 0; i < 2000; ++i) {
            if (lane == 0) {
                transfer(a, b, 1);
            } else {
                transfer(b, a, 1);
            }
        }
    });
    EXPECT_EQ(a.balance(), 2000);
    EXPECT_EQ(b.balance(), 2000);
}

TEST(Part3Routes, ReplaceAndLookup) {
    RouteTable routes;
    EXPECT_EQ(routes.size(), 0u);

    routes.replace({{"/a", "svc-a"}, {"/b", "svc-b"}});
    EXPECT_EQ(routes.size(), 2u);
    EXPECT_EQ(routes.lookup("/a"), "svc-a");
    EXPECT_FALSE(routes.lookup("/missing").has_value());

    routes.replace({});
    EXPECT_EQ(routes.size(), 0u);
    EXPECT_FALSE(routes.lookup("/a").has_value());
}

TEST(Part3Routes, ReadersAlwaysSeeAWholeTable) {
    RouteTable routes;
    routes.replace({{"/a", "v1"}, {"/b", "v1"}});

    // Writer and readers run a bounded number of iterations, so nothing can hang.
    Background writer([&] {
        for (int i = 0; i < 2000; ++i) {
            const std::string v = (i % 2 == 0) ? "v1" : "v2";
            routes.replace({{"/a", v}, {"/b", v}});
        }
    });

    parallel_for(3, [&](std::size_t) {
        for (int i = 0; i < 2000; ++i) {
            for (const char* key : {"/a", "/b"}) {
                const auto value = routes.lookup(key);
                ASSERT_TRUE(value.has_value());
                EXPECT_TRUE(*value == "v1" || *value == "v2");
            }
        }
    });
    writer.join();
}
