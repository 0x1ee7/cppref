#include "static_locals.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using namespace cppref::statics;

// --- Part3: function-local statics, thread-safe init and the Meyers singleton ---
// Each TEST runs in its own process under ctest, so "first use" is real in every test.

TEST(Part3StaticLocals, Crc32KnownVector) {
    EXPECT_EQ(crc32("123456789"), 0xCBF43926u);
}

TEST(Part3StaticLocals, Crc32EmptyInputIsZero) {
    EXPECT_EQ(crc32(""), 0u);
}

TEST(Part3StaticLocals, TableIsBuiltExactlyOnceOnFirstUse) {
    EXPECT_EQ(crc_table_build_count(), 0u);
    (void)crc32("a");
    (void)crc32("b");
    EXPECT_EQ(crc_table_build_count(), 1u);
}

TEST(Part3StaticLocals, ConcurrentFirstUseBuildsTableOnce) {
    constexpr int kThreads = 8;
    std::array<std::uint32_t, kThreads> results{};
    std::vector<std::thread> threads;
    threads.reserve(kThreads);
    for (int i = 0; i < kThreads; ++i) {
        threads.emplace_back([&results, i] { results[i] = crc32("123456789"); });
    }
    for (auto& t : threads) t.join();

    for (auto r : results) EXPECT_EQ(r, 0xCBF43926u);
    EXPECT_EQ(crc_table_build_count(), 1u);
}

TEST(Part3StaticLocals, AuditLogIsSingletonAndNotCopyable) {
    EXPECT_EQ(&AuditLog::instance(), &AuditLog::instance());
    EXPECT_FALSE(std::is_copy_constructible_v<AuditLog>);
}

TEST(Part3StaticLocals, AuditLogAppendPreservesOrder) {
    AuditLog& log = AuditLog::instance();
    log.clear();
    log.append("a");
    log.append("b");
    EXPECT_EQ(log.lines(), (std::vector<std::string>{"a", "b"}));
    log.clear();
}

TEST(Part3StaticLocals, AuditLogClearEmptiesTrail) {
    AuditLog& log = AuditLog::instance();
    log.append("x");
    log.clear();
    EXPECT_TRUE(log.lines().empty());
}

TEST(Part3StaticLocals, AuditLogConcurrentAppendsAreAllKept) {
    AuditLog& log = AuditLog::instance();
    log.clear();
    constexpr int kThreads = 4;
    constexpr int kPerThread = 250;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&log] {
            for (int i = 0; i < kPerThread; ++i) log.append("line");
        });
    }
    for (auto& t : threads) t.join();
    EXPECT_EQ(log.lines().size(), static_cast<std::size_t>(kThreads * kPerThread));
    log.clear();
}
