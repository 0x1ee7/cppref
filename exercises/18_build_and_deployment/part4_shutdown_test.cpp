#include "shutdown.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <csignal>
#include <stdexcept>
#include <thread>

namespace dep = cppref::deployment;
using namespace std::chrono_literals;

namespace {

bool wait_until_stopped(const std::stop_token& token, std::chrono::milliseconds limit) {
    const auto deadline = std::chrono::steady_clock::now() + limit;
    while (!token.stop_requested()) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(1ms);
    }
    return true;
}

}  // namespace

TEST(Part4Shutdown, NothingReceivedBeforeAnySignal) {
    dep::SignalGuard guard{SIGINT};
    EXPECT_FALSE(guard.token().stop_requested());
    EXPECT_FALSE(guard.received_signal().has_value());
}

TEST(Part4Shutdown, SigintRequestsStopAndRecordsSignal) {
    dep::SignalGuard guard{SIGINT, SIGTERM};
    std::raise(SIGINT);
    ASSERT_TRUE(wait_until_stopped(guard.token(), 2s));
    EXPECT_EQ(guard.received_signal(), SIGINT);
}

TEST(Part4Shutdown, SigtermRequestsStop) {
    dep::SignalGuard guard{SIGINT, SIGTERM};
    std::raise(SIGTERM);
    ASSERT_TRUE(wait_until_stopped(guard.token(), 2s));
    EXPECT_EQ(guard.received_signal(), SIGTERM);
}

TEST(Part4Shutdown, FirstSignalWins) {
    dep::SignalGuard guard{SIGINT, SIGTERM};
    std::raise(SIGTERM);
    ASSERT_TRUE(wait_until_stopped(guard.token(), 2s));
    std::raise(SIGINT);
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(guard.received_signal(), SIGTERM);
}

TEST(Part4Shutdown, SecondGuardIsRejectedWhileFirstIsAlive) {
    dep::SignalGuard first{SIGINT};
    EXPECT_THROW({ dep::SignalGuard second{SIGINT}; }, std::logic_error);
}

TEST(Part4Shutdown, DestructorRestoresPreviousDisposition) {
    auto previous = std::signal(SIGTERM, SIG_IGN);
    {
        dep::SignalGuard guard{SIGTERM};
    }
    const auto after = std::signal(SIGTERM, SIG_DFL);
    EXPECT_EQ(after, SIG_IGN);
    std::signal(SIGTERM, previous);
}

TEST(Part4Shutdown, GuardCanBeCreatedAgainAfterDestruction) {
    { dep::SignalGuard first{SIGINT}; }
    EXPECT_NO_THROW({ dep::SignalGuard second{SIGINT}; });
}
