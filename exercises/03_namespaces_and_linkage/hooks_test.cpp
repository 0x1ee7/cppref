#include "hooks.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

using namespace cppref::hooks;
using ::testing::ElementsAre;

// --- Part6a: RAII guards around process-wide state ---

TEST(Part6Hooks, EmitWithoutHookDoesNotThrow) {
    EXPECT_NO_THROW(emit_log("x"));
}

TEST(Part6Hooks, HookReceivesMessage) {
    std::vector<std::string> seen;
    {
        ScopedLogHook h([&](std::string_view m) { seen.emplace_back(m); });
        emit_log("hello");
        emit_log("");
    }
    EXPECT_THAT(seen, ElementsAre("hello", ""));
}

TEST(Part6Hooks, HookIsRemovedOnScopeExit) {
    std::vector<std::string> seen;
    {
        ScopedLogHook h([&](std::string_view m) { seen.emplace_back(m); });
    }
    emit_log("after");
    EXPECT_TRUE(seen.empty());
}

TEST(Part6Hooks, InnerHookWinsAndOuterIsRestored) {
    std::vector<std::string> outer;
    std::vector<std::string> inner;
    ScopedLogHook o([&](std::string_view m) { outer.emplace_back(m); });
    emit_log("a");
    {
        ScopedLogHook i([&](std::string_view m) { inner.emplace_back(m); });
        emit_log("b");
    }
    emit_log("c");
    EXPECT_THAT(outer, ElementsAre("a", "c"));
    EXPECT_THAT(inner, ElementsAre("b"));
}

TEST(Part6Hooks, ScopedLogHookIsNotCopyableOrMovable) {
    ScopedLogHook h([](std::string_view) {});
    EXPECT_FALSE(std::is_copy_constructible_v<ScopedLogHook>);
    EXPECT_FALSE(std::is_move_constructible_v<ScopedLogHook>);
}
