// Parts 1, 2 and 5: levels and hierarchy, lazy formatting and call-site capture,
// MDC scopes. Each test builds its own Registry; there is no shared state.
#include "logging.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <format>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <type_traits>
#include <vector>

namespace cppref::logging::testing {

// Formatting a Costly bumps *calls, so tests can count formatter invocations.
struct Costly {
    std::shared_ptr<int> calls;
    int id;
};

}  // namespace cppref::logging::testing

template <>
struct std::formatter<cppref::logging::testing::Costly> : std::formatter<int> {
    auto format(const cppref::logging::testing::Costly& c, std::format_context& ctx) const {
        ++*c.calls;
        return std::formatter<int>::format(c.id, ctx);
    }
};

namespace {

using cppref::logging::Field;
using cppref::logging::Level;
using cppref::logging::Logger;
using cppref::logging::MdcScope;
using cppref::logging::ParseLevelError;
using cppref::logging::Registry;
using cppref::logging::Sink;
using cppref::logging::mdc_snapshot;
using cppref::logging::parse_level;
using cppref::logging::testing::CaptureSink;
using cppref::logging::testing::Costly;

}  // namespace

// ---------------------------------------------------------------------------
// Part 1: levels, parsing, hierarchy and per-logger inheritance
// ---------------------------------------------------------------------------

TEST(Part1Levels, ParseIsCaseInsensitiveAndAcceptsWarning) {
    EXPECT_EQ(parse_level("INFO"), Level::info);
    EXPECT_EQ(parse_level("Debug"), Level::debug);
    EXPECT_EQ(parse_level("warning"), Level::warn);
    EXPECT_EQ(parse_level("trace"), Level::trace);
    EXPECT_EQ(parse_level("error"), Level::error);
    EXPECT_EQ(parse_level("off"), Level::off);
}

TEST(Part1Levels, ParseRejectsUnknownAndEmptyAndEchoesInput) {
    auto bad = parse_level("verbose");
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error().input, "verbose");
    EXPECT_FALSE(parse_level("").has_value());
}

TEST(Part1Levels, NamesAndOrdering) {
    EXPECT_EQ(cppref::logging::to_string(Level::warn), "WARN");
    EXPECT_EQ(cppref::logging::to_string(Level::off), "OFF");
    EXPECT_LT(Level::trace, Level::debug);
    EXPECT_LT(Level::debug, Level::info);
    EXPECT_LT(Level::info, Level::warn);
    EXPECT_LT(Level::warn, Level::error);
    EXPECT_LT(Level::error, Level::off);
}

TEST(Part1Hierarchy, GetReturnsSameInstanceForSameName) {
    Registry reg;
    EXPECT_EQ(&reg.get("app.db"), &reg.get("app.db"));
    EXPECT_EQ(reg.get("app.db").name(), "app.db");
}

TEST(Part1Hierarchy, GetBuildsTheParentChain) {
    Registry reg;
    Logger& pool = reg.get("app.db.pool");
    ASSERT_NE(pool.parent(), nullptr);
    EXPECT_EQ(pool.parent()->name(), "app.db");
    ASSERT_NE(pool.parent()->parent(), nullptr);
    EXPECT_EQ(pool.parent()->parent()->name(), "app");
    EXPECT_EQ(pool.parent()->parent()->parent(), &reg.root());
    EXPECT_EQ(reg.root().parent(), nullptr);
}

TEST(Part1Hierarchy, RootIsUnnamedAndExplicitlyInfo) {
    Registry reg;
    EXPECT_EQ(reg.root().name(), "");
    EXPECT_EQ(reg.root().level(), Level::info);
    EXPECT_EQ(reg.root().effective_level(), Level::info);
}

TEST(Part1Hierarchy, ChildInheritsEffectiveLevelFromAncestor) {
    Registry reg;
    reg.get("app").set_level(Level::warn);
    Logger& db = reg.get("app.db");
    EXPECT_FALSE(db.level().has_value());
    EXPECT_EQ(db.effective_level(), Level::warn);
}

TEST(Part1Hierarchy, ExplicitChildLevelOverridesAncestorAndClearRestores) {
    Registry reg;
    reg.get("app").set_level(Level::warn);
    Logger& db = reg.get("app.db");
    db.set_level(Level::debug);
    EXPECT_EQ(db.level(), Level::debug);
    EXPECT_EQ(db.effective_level(), Level::debug);
    db.clear_level();
    EXPECT_FALSE(db.level().has_value());
    EXPECT_EQ(db.effective_level(), Level::warn);
}

TEST(Part1Hierarchy, ExistingChildSeesLaterParentChange) {
    Registry reg;
    Logger& db = reg.get("app.db");
    reg.get("app").set_level(Level::error);
    EXPECT_EQ(db.effective_level(), Level::error);
}

TEST(Part1Hierarchy, PrefixMatchingUsesDottedSegmentsOnly) {
    Registry reg;
    reg.get("app").set_level(Level::error);
    EXPECT_EQ(reg.get("application").effective_level(), Level::info);
}

TEST(Part1Hierarchy, InvalidNamesThrowInvalidArgument) {
    Registry reg;
    for (std::string_view name : {"", "a..b", ".a", "a."}) {
        EXPECT_THROW(reg.get(name), std::invalid_argument) << "name: '" << name << "'";
    }
}

TEST(Part1Hierarchy, EnabledFollowsEffectiveLevel) {
    Registry reg;
    Logger& info = reg.get("svc");
    EXPECT_FALSE(info.enabled(Level::debug));
    EXPECT_TRUE(info.enabled(Level::info));
    EXPECT_TRUE(info.enabled(Level::error));

    Logger& silent = reg.get("quiet");
    silent.set_level(Level::off);
    EXPECT_FALSE(silent.enabled(Level::error));
}

TEST(Part1Hierarchy, PropagateDefaultsToTrueAndCanBeCleared) {
    Registry reg;
    Logger& db = reg.get("app.db");
    EXPECT_TRUE(db.propagate());
    db.set_propagate(false);
    EXPECT_FALSE(db.propagate());
}

TEST(Part1Hierarchy, RegistriesAreIndependent) {
    Registry a;
    Registry b;
    a.get("app").set_level(Level::error);
    EXPECT_EQ(b.get("app").effective_level(), Level::info);
    EXPECT_NE(&a.root(), &b.root());
}

// ---------------------------------------------------------------------------
// Part 2: lazy formatting, call-site capture, dispatch through the hierarchy
// ---------------------------------------------------------------------------

TEST(Part2Lazy, DisabledLevelDoesNotFormatArguments) {
    Registry reg;
    CaptureSink sink;
    reg.get("app").add_sink(std::shared_ptr<Sink>(&sink, [](Sink*) {}));
    Logger& log = reg.get("app");  // info by default; debug is disabled
    auto calls = std::make_shared<int>(0);
    Costly c{calls, 7};

    CPPREF_DEBUG(log, "value {}", c);

    EXPECT_EQ(*calls, 0);
    EXPECT_TRUE(sink.records().empty());
}

TEST(Part2Lazy, EnabledLevelFormatsExactlyOnce) {
    Registry reg;
    CaptureSink sink;
    reg.get("app").add_sink(std::shared_ptr<Sink>(&sink, [](Sink*) {}));
    Logger& log = reg.get("app");
    auto calls = std::make_shared<int>(0);

    CPPREF_INFO(log, "value {}", Costly{calls, 7});

    EXPECT_EQ(*calls, 1);
    auto records = sink.records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].message, "value 7");
}

TEST(Part2Lazy, MacrosMapToTheirLevels) {
    Registry reg;
    CaptureSink sink;
    reg.get("app").add_sink(std::shared_ptr<Sink>(&sink, [](Sink*) {}));
    Logger& log = reg.get("app");
    log.set_level(Level::trace);

    CPPREF_TRACE(log, "t");
    CPPREF_DEBUG(log, "d");
    CPPREF_INFO(log, "i");
    CPPREF_WARN(log, "w");
    CPPREF_ERROR(log, "e");

    std::vector<Level> levels;
    for (const auto& r : sink.records()) levels.push_back(r.level);
    EXPECT_EQ(levels, (std::vector<Level>{Level::trace, Level::debug, Level::info,
                                          Level::warn, Level::error}));
}

TEST(Part2Lazy, CapturesCallSiteLocation) {
    Registry reg;
    CaptureSink sink;
    reg.get("app").add_sink(std::shared_ptr<Sink>(&sink, [](Sink*) {}));
    Logger& log = reg.get("app");

    const auto expected_line = __LINE__ + 1;
    CPPREF_INFO(log, "where");

    auto records = sink.records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].location.line(), static_cast<std::uint_least32_t>(expected_line));
    EXPECT_TRUE(std::string_view(records[0].location.file_name()).ends_with("logging_test.cpp"));
}

TEST(Part2Lazy, RecordCarriesLoggerNameTimeAndThread) {
    Registry reg;
    CaptureSink sink;
    reg.get("app").add_sink(std::shared_ptr<Sink>(&sink, [](Sink*) {}));
    CPPREF_INFO(reg.get("app.db"), "hi");

    auto records = sink.records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].logger, "app.db");
    EXPECT_GT(records[0].time.time_since_epoch().count(), 0);
    EXPECT_EQ(records[0].thread, std::this_thread::get_id());
}

TEST(Part2Lazy, MessageUsesStdFormatSpecsAndEscapes) {
    Registry reg;
    CaptureSink sink;
    reg.get("app").add_sink(std::shared_ptr<Sink>(&sink, [](Sink*) {}));
    CPPREF_INFO(reg.get("app"), "{:>5}|{{literal}}|{:.2f}", 42, 3.14159);

    auto records = sink.records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].message, "   42|{literal}|3.14");
}

TEST(Part2Lazy, ChildRecordsReachAncestorSinks) {
    Registry reg;
    CaptureSink sink;
    reg.get("app").add_sink(std::shared_ptr<Sink>(&sink, [](Sink*) {}));
    CPPREF_INFO(reg.get("app.db"), "from child");

    auto records = sink.records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].logger, "app.db");
}

TEST(Part2Lazy, RootSinkReceivesRecordsWhilePropagating) {
    Registry reg;
    CaptureSink sink;
    reg.root().add_sink(std::shared_ptr<Sink>(&sink, [](Sink*) {}));
    CPPREF_WARN(reg.get("a.b.c"), "deep");
    EXPECT_EQ(sink.records().size(), 1u);
}

TEST(Part2Lazy, PropagateFalseStopsAtTheLoggerWithItsOwnSinks) {
    Registry reg;
    CaptureSink child_sink;
    CaptureSink parent_sink;
    reg.get("app").add_sink(std::shared_ptr<Sink>(&parent_sink, [](Sink*) {}));
    Logger& db = reg.get("app.db");
    db.add_sink(std::shared_ptr<Sink>(&child_sink, [](Sink*) {}));
    db.set_propagate(false);

    CPPREF_INFO(db, "local only");

    EXPECT_EQ(child_sink.records().size(), 1u);
    EXPECT_TRUE(parent_sink.records().empty());
}

TEST(Part2Lazy, EachSinkAlongTheChainReceivesItsOwnCopy) {
    Registry reg;
    CaptureSink child_sink;
    CaptureSink parent_sink;
    reg.get("app").add_sink(std::shared_ptr<Sink>(&parent_sink, [](Sink*) {}));
    reg.get("app.db").add_sink(std::shared_ptr<Sink>(&child_sink, [](Sink*) {}));

    CPPREF_INFO(reg.get("app.db"), "both");

    EXPECT_EQ(child_sink.records().size(), 1u);
    EXPECT_EQ(parent_sink.records().size(), 1u);
}

TEST(Part2Lazy, LevelFilterAppliesBeforeAnySinkSeesTheRecord) {
    Registry reg;
    CaptureSink sink;
    reg.get("app").add_sink(std::shared_ptr<Sink>(&sink, [](Sink*) {}));
    Logger& db = reg.get("app.db");
    db.set_level(Level::warn);

    CPPREF_INFO(db, "dropped");
    CPPREF_WARN(db, "kept");

    auto records = sink.records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].message, "kept");
}

// ---------------------------------------------------------------------------
// Part 5: MDC scopes (thread-local, nested, exception-safe)
// ---------------------------------------------------------------------------

TEST(Part5Mdc, ScopeAddsFieldToRecordsLoggedInside) {
    Registry reg;
    CaptureSink sink;
    reg.get("app").add_sink(std::shared_ptr<Sink>(&sink, [](Sink*) {}));
    Logger& log = reg.get("app");
    {
        MdcScope scope("request_id", "r-1");
        CPPREF_INFO(log, "inside");
    }
    CPPREF_INFO(log, "outside");

    auto records = sink.records();
    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[0].fields, (std::vector<Field>{{"request_id", "r-1"}}));
    EXPECT_TRUE(records[1].fields.empty());
}

TEST(Part5Mdc, NoScopeMeansEmptySnapshot) {
    EXPECT_TRUE(mdc_snapshot().empty());
}

TEST(Part5Mdc, SnapshotOrdersOutermostFirstAndKeepsDistinctKeys) {
    MdcScope outer("a", "1");
    MdcScope inner("b", "2");
    EXPECT_EQ(mdc_snapshot(), (std::vector<Field>{{"a", "1"}, {"b", "2"}}));
}

TEST(Part5Mdc, NestedScopeShadowsSameKeyAndRestoresOnExit) {
    Registry reg;
    CaptureSink sink;
    reg.get("app").add_sink(std::shared_ptr<Sink>(&sink, [](Sink*) {}));
    Logger& log = reg.get("app");
    {
        MdcScope outer("k", "a");
        {
            MdcScope inner("k", "b");
            CPPREF_INFO(log, "shadowed");
        }
        CPPREF_INFO(log, "restored");
    }

    auto records = sink.records();
    ASSERT_EQ(records.size(), 2u);
    EXPECT_EQ(records[0].fields, (std::vector<Field>{{"k", "b"}}));
    EXPECT_EQ(records[1].fields, (std::vector<Field>{{"k", "a"}}));
}

TEST(Part5Mdc, ScopeRestoresStateWhenExceptionUnwinds) {
    try {
        MdcScope scope("k", "v");
        throw std::runtime_error("boom");
    } catch (const std::runtime_error&) {
    }
    EXPECT_TRUE(mdc_snapshot().empty());
}

TEST(Part5Mdc, OtherThreadsDoNotSeeTheScope) {
    Registry reg;
    CaptureSink sink;
    reg.get("app").add_sink(std::shared_ptr<Sink>(&sink, [](Sink*) {}));
    Logger& log = reg.get("app");
    MdcScope scope("request_id", "main-only");

    std::thread worker([&log] { CPPREF_INFO(log, "from worker"); });
    worker.join();

    auto records = sink.records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_TRUE(records[0].fields.empty());
    EXPECT_EQ(mdc_snapshot(), (std::vector<Field>{{"request_id", "main-only"}}));
}

TEST(Part5Mdc, ScopeIsNeitherCopyableNorMovable) {
    MdcScope scope("request", "42");
    const auto snapshot = mdc_snapshot();
    ASSERT_EQ(snapshot.size(), 1u);
    EXPECT_EQ(snapshot[0].key, "request");
    EXPECT_FALSE(std::is_copy_constructible_v<MdcScope>);
    EXPECT_FALSE(std::is_move_constructible_v<MdcScope>);
}
