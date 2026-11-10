#include "health.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <expected>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace dep = cppref::deployment::health;
using namespace std::chrono_literals;

namespace {

dep::CheckFn ok_check() {
    return [](std::stop_token) -> std::expected<void, std::string> { return {}; };
}

dep::CheckFn fail_check(std::string why) {
    return [why](std::stop_token) -> std::expected<void, std::string> { return std::unexpected(why); };
}

}  // namespace

TEST(Part5Health, EmptyRegistryIsReady) {
    dep::HealthRegistry reg;
    EXPECT_EQ(reg.size(), 0u);
    const auto reports = reg.run_all();
    EXPECT_TRUE(reports.empty());
    EXPECT_TRUE(dep::all_ok(reports));
    EXPECT_EQ(dep::to_json(reports), R"({"status":"ready","checks":{}})");
}

TEST(Part5Health, AddValidatesArguments) {
    dep::HealthRegistry reg;
    reg.add("db", ok_check(), 1s);
    EXPECT_EQ(reg.size(), 1u);
    EXPECT_THROW(reg.add("db", ok_check(), 1s), std::invalid_argument);
    EXPECT_THROW(reg.add("cache", ok_check(), 0ms), std::invalid_argument);
    EXPECT_EQ(reg.size(), 1u);
}

TEST(Part5Health, RunsInRegistrationOrder) {
    dep::HealthRegistry reg;
    reg.add("db", ok_check(), 1s);
    reg.add("cache", ok_check(), 1s);
    reg.add("queue", ok_check(), 1s);
    const auto reports = reg.run_all();
    ASSERT_EQ(reports.size(), 3u);
    EXPECT_EQ(reports[0].name, "db");
    EXPECT_EQ(reports[1].name, "cache");
    EXPECT_EQ(reports[2].name, "queue");
}

TEST(Part5Health, PassingCheckHasEmptyDetail) {
    dep::HealthRegistry reg;
    reg.add("db", ok_check(), 1s);
    const auto reports = reg.run_all();
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_TRUE(reports[0].ok);
    EXPECT_TRUE(reports[0].detail.empty());
}

TEST(Part5Health, FailureIsReportedAndOthersStillRun) {
    dep::HealthRegistry reg;
    reg.add("db", fail_check("down"), 1s);
    reg.add("cache", ok_check(), 1s);
    const auto reports = reg.run_all();
    ASSERT_EQ(reports.size(), 2u);
    EXPECT_FALSE(reports[0].ok);
    EXPECT_EQ(reports[0].detail, "down");
    EXPECT_TRUE(reports[1].ok);
    EXPECT_FALSE(dep::all_ok(reports));
    EXPECT_EQ(dep::to_json(reports),
              R"({"status":"not_ready","checks":{"db":"error: down","cache":"ok"}})");
}

TEST(Part5Health, ThrowingCheckBecomesFailure) {
    dep::HealthRegistry reg;
    reg.add("boom", [](std::stop_token) -> std::expected<void, std::string> {
        throw std::runtime_error("boom");
    }, 1s);
    const auto reports = reg.run_all();
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_FALSE(reports[0].ok);
    EXPECT_NE(reports[0].detail.find("boom"), std::string::npos) << reports[0].detail;
}

TEST(Part5Health, TimeoutIsEnforcedCooperatively) {
    dep::HealthRegistry reg;
    reg.add("slow", [](std::stop_token st) -> std::expected<void, std::string> {
        while (!st.stop_requested()) std::this_thread::sleep_for(1ms);
        return std::unexpected(std::string("stopped"));
    }, 50ms);
    const auto reports = reg.run_all();
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_FALSE(reports[0].ok);
    EXPECT_EQ(reports[0].detail, "timeout after 50ms");
    EXPECT_LT(reports[0].elapsed, 5s);
}

TEST(Part5Health, CheckSeesLiveStopTokenWhenNotCancelled) {
    dep::HealthRegistry reg;
    reg.add("token", [](std::stop_token st) -> std::expected<void, std::string> {
        if (st.stop_requested()) return std::unexpected(std::string("stop already requested"));
        return {};
    }, 1s);
    const auto reports = reg.run_all();
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_TRUE(reports[0].ok) << reports[0].detail;
}

TEST(Part5Health, JsonEscapesNamesAndDetails) {
    const std::vector<dep::CheckReport> reports{
        {.name = "say\"hi", .ok = false, .detail = std::string("x\n\\y"), .elapsed = 1ms}};
    EXPECT_EQ(dep::to_json(reports), R"({"status":"not_ready","checks":{"say\"hi":"error: x\n\\y"}})");
    EXPECT_EQ(dep::escape_json(std::string_view("\x01", 1)), R"(\u0001)");
}
