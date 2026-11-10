// Part 7: humanized relative time and an injected-clock stopwatch.
#include "humanize.hpp"
#include "stopwatch.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <stdexcept>

namespace {

using namespace cppref::chrono_time;
using namespace std::chrono_literals;
using cppref::chrono_time::fixture::utc;
using cppref::chrono_time::fixture::zoned;

// Fake clock: time only moves when the test says so.
struct FakeClock {
    using rep = std::chrono::nanoseconds::rep;
    using period = std::chrono::nanoseconds::period;
    using duration = std::chrono::nanoseconds;
    using time_point = std::chrono::time_point<FakeClock>;
    static constexpr bool is_steady = true;
    static inline duration now_value{0};
    static time_point now() { return time_point{now_value}; }
};

// Berlin 10 June 2026 12:00 CEST = 10:00Z.
ZonedInstant berlinNow() { return zoned("Europe/Berlin", utc(2026, 6, 10, 10)); }

ZonedInstant at(Instant instant) { return zoned("Europe/Berlin", instant); }

TEST(Part7Humanize, JustNowWithinFortyFiveSeconds) {
    EXPECT_EQ(humanize(at(utc(2026, 6, 10, 10) + 44s), berlinNow()), "just now");
    EXPECT_EQ(humanize(at(utc(2026, 6, 10, 10) - 44s), berlinNow()), "just now");
}

TEST(Part7Humanize, MinutesFutureAndPastSingularAndPlural) {
    EXPECT_EQ(humanize(at(utc(2026, 6, 10, 10) + 45s), berlinNow()), "in 1 minute");
    EXPECT_EQ(humanize(at(utc(2026, 6, 10, 10) - 60s), berlinNow()), "1 minute ago");
    EXPECT_EQ(humanize(at(utc(2026, 6, 10, 10) - (59min + 59s)), berlinNow()), "59 minutes ago");
    EXPECT_EQ(humanize(at(utc(2026, 6, 10, 10) + 59min), berlinNow()), "in 59 minutes");
}

TEST(Part7Humanize, HoursFutureAndPast) {
    EXPECT_EQ(humanize(at(utc(2026, 6, 10, 10) - 3h), berlinNow()), "3 hours ago");
    EXPECT_EQ(humanize(at(utc(2026, 6, 10, 10) + 1h), berlinNow()), "in 1 hour");
}

TEST(Part7Humanize, DaysUnderThirtyFromTheAbsoluteDifference) {
    EXPECT_EQ(humanize(at(utc(2026, 6, 10, 10) - 48h), berlinNow()), "2 days ago");
    EXPECT_EQ(humanize(at(utc(2026, 6, 10, 10) + 24h), berlinNow()), "in 1 day");
    EXPECT_EQ(humanize(at(utc(2026, 6, 10, 10) + 48h), berlinNow()), "in 2 days");
}

TEST(Part7Humanize, OlderThanThirtyDaysUsesDateInWhenZone) {
    EXPECT_EQ(humanize(at(utc(2026, 6, 10, 10) - 31 * 24h), berlinNow()), "on 2026-05-10");
}

TEST(Part7Humanize, DateUsesTheZoneOfTheInstant) {
    // 2026-05-10 22:30Z is already 11 May in Tokyo.
    const auto tokyoWhen = zoned("Asia/Tokyo", utc(2026, 5, 10, 22, 30));
    EXPECT_EQ(humanize(tokyoWhen, berlinNow()), "on 2026-05-11");
}

TEST(Part7Humanize, DstWallClockIsNotTheElapsedTime) {
    // now = 2026-03-29 00:30Z (01:30 CET). when = 01:30Z (03:30 CEST, 1 absolute hour later).
    const auto now = zoned("Europe/Berlin", utc(2026, 3, 29, 0, 30));
    EXPECT_EQ(humanize(zoned("Europe/Berlin", utc(2026, 3, 29, 1, 30)), now), "in 1 hour");
}

TEST(Part7Stopwatch, ElapsedIsZeroBeforeStart) {
    FakeClock::now_value = 0ns;
    Stopwatch<FakeClock> sw;
    EXPECT_EQ(sw.elapsed(), 0ns);
}

TEST(Part7Stopwatch, StartStopMeasuresAndFreezes) {
    FakeClock::now_value = 100ms;
    Stopwatch<FakeClock> sw;
    sw.start();
    FakeClock::now_value = 350ms;
    EXPECT_EQ(sw.stop(), 250ms);
    FakeClock::now_value = 999ms;
    EXPECT_EQ(sw.elapsed(), 250ms);
}

TEST(Part7Stopwatch, ElapsedIsLiveWhileRunning) {
    FakeClock::now_value = 0ns;
    Stopwatch<FakeClock> sw;
    sw.start();
    FakeClock::now_value = 40ms;
    EXPECT_EQ(sw.elapsed(), 40ms);
}

TEST(Part7Stopwatch, StartTwiceAndStopWithoutStartThrow) {
    Stopwatch<FakeClock> sw;
    EXPECT_THROW(sw.stop(), std::logic_error);
    sw.start();
    EXPECT_THROW(sw.start(), std::logic_error);
}

TEST(Part7Stopwatch, RestartAfterStopMeasuresAgain) {
    FakeClock::now_value = 0ns;
    Stopwatch<FakeClock> sw;
    sw.start();
    FakeClock::now_value = 10ms;
    (void)sw.stop();
    FakeClock::now_value = 100ms;
    sw.start();
    FakeClock::now_value = 130ms;
    EXPECT_EQ(sw.stop(), 30ms);
}

TEST(Part7Stopwatch, SteadyClockConceptRejectsSystemClock) {
    EXPECT_TRUE(SteadyClock<std::chrono::steady_clock>);
    EXPECT_FALSE(SteadyClock<std::chrono::system_clock>);
    // Behavioural check (the concept above is compile-time only): a Stopwatch over a
    // steady clock can be started and stopped.
    Stopwatch<FakeClock> probe;
    probe.start();
    probe.stop();
}

}  // namespace
