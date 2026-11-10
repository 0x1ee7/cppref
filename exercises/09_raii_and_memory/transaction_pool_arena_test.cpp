// Part 6-7: undo-log transactions with savepoints, RAII connection leases, pmr arena tokenizer.
#include "arena_parser.hpp"
#include "connection_pool.hpp"
#include "transaction.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <memory_resource>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace rr = cppref::raii_and_memory;

namespace {
using rr::KvMap;

std::optional<std::string> opt(const char* s) { return std::optional<std::string>(s); }

KvMap seeded() {
    KvMap m;
    m.emplace("a", "1");
    m.emplace("x", "9");
    return m;
}

// ---------------------------------------------------------------------------
// Part 6 - Transaction with undo journal and savepoints
// ---------------------------------------------------------------------------
class Part6Transaction : public ::testing::Test {};

TEST_F(Part6Transaction, WritesAreVisibleThroughTransactionAndTarget) {
    KvMap m = seeded();
    rr::Transaction t(m);
    t.put("b", "2");
    EXPECT_EQ(t.get("b"), opt("2"));
    EXPECT_EQ(m.at("b"), "2");
}

TEST_F(Part6Transaction, GetMissingKeyIsNullopt) {
    KvMap m = seeded();
    rr::Transaction t(m);
    EXPECT_FALSE(t.get("zz").has_value());
}

TEST_F(Part6Transaction, CommitKeepsChangesAndEmptiesJournal) {
    KvMap m = seeded();
    {
        rr::Transaction t(m);
        t.put("b", "2");
        t.commit();
        EXPECT_FALSE(t.active());
        EXPECT_EQ(t.journal_size(), 0u);
    }
    EXPECT_EQ(m.at("b"), "2");
}

TEST_F(Part6Transaction, DestructorRollsBackUncommitted) {
    KvMap m = seeded();
    const KvMap before = m;
    {
        rr::Transaction t(m);
        t.put("a", "X");
        t.put("c", "3");
        t.erase("x");
    }
    EXPECT_EQ(m, before);
}

TEST_F(Part6Transaction, ExplicitRollbackRestoresOverwrittenAndErased) {
    KvMap m = seeded();
    const KvMap before = m;
    rr::Transaction t(m);
    t.put("a", "changed");
    EXPECT_TRUE(t.erase("x"));
    t.rollback();
    EXPECT_EQ(m, before);
    EXPECT_FALSE(t.active());
}

TEST_F(Part6Transaction, RollbackIsIdempotent) {
    KvMap m = seeded();
    const KvMap before = m;
    rr::Transaction t(m);
    t.put("k", "v");
    t.rollback();
    t.rollback();
    EXPECT_EQ(m, before);
}

TEST_F(Part6Transaction, EraseMissingKeyReturnsFalseAndRecordsNothing) {
    KvMap m = seeded();
    rr::Transaction t(m);
    EXPECT_FALSE(t.erase("absent"));
    EXPECT_EQ(t.journal_size(), 0u);
}

TEST_F(Part6Transaction, JournalCountsOneEntryPerMutation) {
    KvMap m = seeded();
    rr::Transaction t(m);
    t.put("a", "2");
    t.put("new", "n");
    EXPECT_EQ(t.journal_size(), 2u);
}

TEST_F(Part6Transaction, ActiveUntilCommitted) {
    KvMap m = seeded();
    rr::Transaction t(m);
    EXPECT_TRUE(t.active());
}

TEST_F(Part6Transaction, ExceptionEscapingScopeRollsBack) {
    KvMap m = seeded();
    const KvMap before = m;
    try {
        rr::Transaction t(m);
        t.put("a", "X");
        throw std::runtime_error("disk full");
    } catch (const std::runtime_error&) {
    }
    EXPECT_EQ(m, before);
}

TEST_F(Part6Transaction, SavepointRollbackUndoesOnlyLaterWork) {
    KvMap m = seeded();
    rr::Transaction t(m);
    t.put("x2", "kept");
    auto sp = t.savepoint();
    t.put("y", "gone");
    sp.rollback();
    EXPECT_FALSE(m.contains("y"));
    EXPECT_EQ(m.at("x2"), "kept");
    t.commit();
    EXPECT_EQ(m.at("x2"), "kept");
    EXPECT_FALSE(m.contains("y"));
}

TEST_F(Part6Transaction, SavepointDestructorRollsBackToMark) {
    KvMap m = seeded();
    rr::Transaction t(m);
    {
        auto sp = t.savepoint();
        t.put("y", "gone");
    }
    EXPECT_FALSE(m.contains("y"));
}

TEST_F(Part6Transaction, SavepointReleaseKeepsChangesButEnclosingRollbackUndoesThem) {
    KvMap m = seeded();
    const KvMap before = m;
    rr::Transaction t(m);
    {
        auto sp = t.savepoint();
        t.put("y", "kept-for-now");
        sp.release();
    }
    EXPECT_EQ(m.at("y"), "kept-for-now");
    t.rollback();
    EXPECT_EQ(m, before);
}

TEST_F(Part6Transaction, NestedSavepointsUnwindInOrder) {
    KvMap m = seeded();
    rr::Transaction t(m);
    auto outer = t.savepoint();
    t.put("a", "2");
    auto inner = t.savepoint();
    t.put("b", "3");
    inner.rollback();
    EXPECT_EQ(m.at("a"), "2");
    EXPECT_FALSE(m.contains("b"));
    outer.rollback();
    EXPECT_EQ(m.at("a"), "1");
}

TEST_F(Part6Transaction, SavepointDestructorAfterRollbackDoesNotUndoLaterWork) {
    KvMap m = seeded();
    rr::Transaction t(m);
    {
        auto sp = t.savepoint();
        t.put("y", "undone");
        sp.rollback();
        t.put("z", "survives");
    }
    EXPECT_EQ(m.at("z"), "survives");
}

TEST_F(Part6Transaction, MovedFromSavepointIsInert) {
    KvMap m = seeded();
    rr::Transaction t(m);
    auto a = t.savepoint();
    t.put("y", "undone-by-b");
    {
        rr::Savepoint b(std::move(a));
    }
    EXPECT_FALSE(m.contains("y"));
    t.put("z", "after");
    EXPECT_EQ(m.at("z"), "after");
}

TEST_F(Part6Transaction, SavepointIsNotCopyable) {
    static_assert(!std::is_copy_constructible_v<rr::Savepoint>);
    static_assert(std::is_nothrow_move_constructible_v<rr::Savepoint>);
    KvMap m;
    rr::Transaction t(m);
    rr::Savepoint sp = t.savepoint();
    rr::Savepoint moved(std::move(sp));
    t.put("k", "v");
    EXPECT_EQ(m.at("k"), "v");
    t.commit();
}

// ---------------------------------------------------------------------------
// Part 7 - connection pool leases and arena-backed tokenizer
// ---------------------------------------------------------------------------
class Part7Pool : public ::testing::Test {
protected:
    int created = 0;
    rr::ConnectionPool::Factory factory() {
        return [this]() {
            ++created;
            return std::make_unique<rr::Connection>(created);
        };
    }
};

TEST_F(Part7Pool, AcquireCreatesLazily) {
    rr::ConnectionPool pool(2, factory());
    EXPECT_EQ(created, 0);
    auto l = pool.try_acquire();
    ASSERT_TRUE(l.has_value());
    EXPECT_EQ(created, 1);
    EXPECT_EQ(pool.live_count(), 1u);
    EXPECT_EQ(pool.idle_count(), 0u);
}

TEST_F(Part7Pool, LeaseReturnsConnectionToIdleOnScopeExit) {
    rr::ConnectionPool pool(2, factory());
    {
        auto l = pool.try_acquire();
        ASSERT_TRUE(l.has_value());
    }
    EXPECT_EQ(pool.idle_count(), 1u);
    EXPECT_EQ(pool.live_count(), 1u);
}

TEST_F(Part7Pool, ReusesIdleConnectionWithoutFactoryCall) {
    rr::ConnectionPool pool(2, factory());
    int first_id = 0;
    {
        auto l = pool.try_acquire();
        first_id = (*l)->id();
    }
    auto again = pool.try_acquire();
    ASSERT_TRUE(again.has_value());
    EXPECT_EQ((*again)->id(), first_id);
    EXPECT_EQ(created, 1);
}

TEST_F(Part7Pool, ExhaustedPoolReturnsNullopt) {
    rr::ConnectionPool pool(2, factory());
    auto a = pool.try_acquire();
    auto b = pool.try_acquire();
    ASSERT_TRUE(a && b);
    EXPECT_FALSE(pool.try_acquire().has_value());
    EXPECT_EQ(pool.live_count(), 2u);
}

TEST_F(Part7Pool, ReleasingFreesSlot) {
    rr::ConnectionPool pool(1, factory());
    {
        auto a = pool.try_acquire();
        EXPECT_FALSE(pool.try_acquire().has_value());
    }
    EXPECT_TRUE(pool.try_acquire().has_value());
}

TEST_F(Part7Pool, BrokenConnectionIsDiscardedNotReturned) {
    rr::ConnectionPool pool(1, factory());
    {
        auto l = pool.try_acquire();
        (*l)->mark_broken();
    }
    EXPECT_EQ(pool.idle_count(), 0u);
    EXPECT_EQ(pool.live_count(), 0u);
    auto fresh = pool.try_acquire();
    ASSERT_TRUE(fresh.has_value());
    EXPECT_EQ(created, 2);
    EXPECT_FALSE((*fresh)->broken());
}

TEST_F(Part7Pool, LeaseQueriesThroughConnection) {
    rr::ConnectionPool pool(1, factory());
    auto l = pool.try_acquire();
    ASSERT_TRUE(l.has_value());
    EXPECT_EQ((*l)->query("SELECT 1"), "1:SELECT 1");
}

TEST_F(Part7Pool, QueryOnBrokenConnectionThrows) {
    rr::ConnectionPool pool(1, factory());
    auto l = pool.try_acquire();
    (*l)->mark_broken();
    EXPECT_THROW((*l)->query("SELECT 1"), std::runtime_error);
}

TEST_F(Part7Pool, MovedLeaseTransfersReturnDuty) {
    rr::ConnectionPool pool(2, factory());
    auto a = pool.try_acquire();
    ASSERT_TRUE(a.has_value());
    {
        rr::ConnectionPool::Lease b(std::move(*a));
        EXPECT_FALSE(*a);
        EXPECT_EQ(pool.idle_count(), 0u);
    }
    EXPECT_EQ(pool.idle_count(), 1u);
    EXPECT_EQ(pool.live_count(), 1u);
}

TEST_F(Part7Pool, LeaseOutlivingPoolStaysUsableAndReturnsSafely) {
    std::optional<rr::ConnectionPool::Lease> held;
    {
        rr::ConnectionPool pool(1, factory());
        held = pool.try_acquire();
    }
    ASSERT_TRUE(held.has_value());
    EXPECT_EQ((*held)->query("PING"), "1:PING");
    held.reset();  // must not touch the destroyed pool object
}

TEST_F(Part7Pool, FactoryExceptionLeavesPoolUnchanged) {
    bool fail = true;
    rr::ConnectionPool pool(1, [&]() -> std::unique_ptr<rr::Connection> {
        if (fail) throw std::runtime_error("cannot connect");
        return std::make_unique<rr::Connection>(1);
    });
    EXPECT_THROW(pool.try_acquire(), std::runtime_error);
    EXPECT_EQ(pool.live_count(), 0u);
    fail = false;
    EXPECT_TRUE(pool.try_acquire().has_value());
}

TEST_F(Part7Pool, LeaseIsMoveOnly) {
    static_assert(!std::is_copy_constructible_v<rr::ConnectionPool::Lease>);
    static_assert(std::is_nothrow_move_constructible_v<rr::ConnectionPool::Lease>);
    rr::ConnectionPool pool(1, factory());
    auto a = pool.try_acquire();
    ASSERT_TRUE(a.has_value());
    rr::ConnectionPool::Lease moved(std::move(*a));
    EXPECT_TRUE(moved);
    EXPECT_FALSE(*a);
}

// Counts allocations made through it; delegates storage to new/delete.
class CountingResource final : public std::pmr::memory_resource {
public:
    std::size_t allocs = 0;

private:
    void* do_allocate(std::size_t bytes, std::size_t align) override {
        ++allocs;
        return std::pmr::new_delete_resource()->allocate(bytes, align);
    }
    void do_deallocate(void* p, std::size_t bytes, std::size_t align) override {
        std::pmr::new_delete_resource()->deallocate(p, bytes, align);
    }
    bool do_is_equal(const memory_resource& other) const noexcept override { return this == &other; }
};

class Part7Arena : public ::testing::Test {};

TEST_F(Part7Arena, SplitsOnAnyWhitespace) {
    std::pmr::monotonic_buffer_resource arena;
    auto r = rr::tokenize("  alpha\tbeta\n gamma  ", &arena);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r->size(), 3u);
    EXPECT_EQ((*r)[0].text, "alpha");
    EXPECT_EQ((*r)[1].text, "beta");
    EXPECT_EQ((*r)[2].text, "gamma");
}

TEST_F(Part7Arena, LineNumbersStartAtOneAndCountNewlines) {
    std::pmr::monotonic_buffer_resource arena;
    auto r = rr::tokenize("a\n\nb", &arena);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r->size(), 2u);
    EXPECT_EQ((*r)[0].line, 1u);
    EXPECT_EQ((*r)[1].line, 3u);
}

TEST_F(Part7Arena, QuotedTokenKeepsSpacesAndDropsQuotes) {
    std::pmr::monotonic_buffer_resource arena;
    auto r = rr::tokenize(R"(set "hello world" x)", &arena);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r->size(), 3u);
    EXPECT_EQ((*r)[1].text, "hello world");
    EXPECT_EQ((*r)[2].text, "x");
}

TEST_F(Part7Arena, EmptyInputYieldsNoTokens) {
    std::pmr::monotonic_buffer_resource arena;
    auto r = rr::tokenize("   \n\t ", &arena);
    ASSERT_TRUE(r.has_value());
    EXPECT_TRUE(r->empty());
}

TEST_F(Part7Arena, UnterminatedQuoteReportsLine) {
    std::pmr::monotonic_buffer_resource arena;
    auto r = rr::tokenize("ok\n\"open", &arena);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error().line, 2u);
    EXPECT_EQ(r.error().message, "unterminated quote");
}

TEST_F(Part7Arena, NewlineInsideQuoteIsError) {
    std::pmr::monotonic_buffer_resource arena;
    auto r = rr::tokenize("\"a\nb\"", &arena);
    EXPECT_FALSE(r.has_value());
}

TEST_F(Part7Arena, VectorAndTextUseTheArena) {
    CountingResource res;
    std::pmr::monotonic_buffer_resource arena(&res);
    auto r = rr::tokenize("short and a token that is definitely longer than sso capacity", &arena);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->get_allocator().resource(), &arena);
    for (const auto& t : *r) EXPECT_EQ(t.text.get_allocator().resource(), &arena);
    EXPECT_GT(res.allocs, 0u);
}

TEST_F(Part7Arena, ManyTokensStayValid) {
    std::pmr::monotonic_buffer_resource arena;
    std::string input;
    for (int i = 0; i < 10000; ++i) input += "tok" + std::to_string(i) + "\n";
    auto r = rr::tokenize(input, &arena);
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r->size(), 10000u);
    EXPECT_EQ(r->front().text, "tok0");
    EXPECT_EQ(r->back().text, "tok9999");
    EXPECT_EQ(r->back().line, 10000u);
}

TEST_F(Part7Arena, MovedVectorKeepsItsArena) {
    std::pmr::monotonic_buffer_resource arena;
    auto r = rr::tokenize("a b", &arena);
    ASSERT_TRUE(r.has_value());
    std::pmr::vector<rr::Token> moved(std::move(*r));
    EXPECT_EQ(moved.get_allocator().resource(), &arena);
    EXPECT_EQ(moved.size(), 2u);
}

}  // namespace
