// 02 · Parameter passing: specification tests. Each suite maps to a Part in README.md.
#include <parameter_passing.hpp>

#include <array>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using namespace std::chrono_literals;
namespace pp = cppref::parameter_passing;

namespace {

// Instrumented payload: counts copies and moves so tests can prove the cost of
// every parameter-passing strategy. Counters are reset at the start of each measurement.
struct Stats {
    static inline int copies = 0;
    static inline int moves = 0;
    static void reset() {
        copies = 0;
        moves = 0;
    }
};

struct Payload {
    std::string bytes;

    explicit Payload(std::string b) : bytes(std::move(b)) {}
    Payload(const Payload& other) : bytes(other.bytes) { ++Stats::copies; }
    Payload(Payload&& other) noexcept : bytes(std::move(other.bytes)) { ++Stats::moves; }
    Payload& operator=(const Payload& other) {
        ++Stats::copies;
        bytes = other.bytes;
        return *this;
    }
    Payload& operator=(Payload&& other) noexcept {
        ++Stats::moves;
        bytes = std::move(other.bytes);
        return *this;
    }
};

// ---------------------------------------------------------------------------
// Part 1
// ---------------------------------------------------------------------------

TEST(Part1Sink, LvalueSinkCostsOneCopyAndOneMove) {
    Payload p{"abc"};
    Stats::reset();
    pp::Slot<Payload> slot(p);  // copy into the parameter, move into the member
    EXPECT_EQ(Stats::copies, 1);
    EXPECT_EQ(Stats::moves, 1);
    EXPECT_EQ(slot.get().bytes, "abc");
    EXPECT_EQ(p.bytes, "abc");
}

TEST(Part1Sink, NamedRvalueSinkCostsTwoMovesNoCopies) {
    Payload p{"abc"};
    Stats::reset();
    pp::Slot<Payload> slot(std::move(p));
    EXPECT_EQ(Stats::copies, 0);
    EXPECT_EQ(Stats::moves, 2);
    EXPECT_EQ(slot.get().bytes, "abc");
}

TEST(Part1Sink, PrvalueIsElidedIntoParameterSoOnlyOneMove) {
    Stats::reset();
    pp::Slot<Payload> slot(Payload{"xyz"});
    EXPECT_EQ(Stats::copies, 0);
    EXPECT_EQ(Stats::moves, 1);
    EXPECT_EQ(slot.get().bytes, "xyz");
}

TEST(Part1Sink, ResetFollowsTheSameSinkCostProfile) {
    Payload first{"one"};
    pp::Slot<Payload> slot(std::move(first));
    Payload next{"two"};
    Stats::reset();
    slot.reset(next);
    EXPECT_EQ(Stats::copies, 1);
    EXPECT_EQ(Stats::moves, 1);
    EXPECT_EQ(slot.get().bytes, "two");
}

TEST(Part1Sink, ConstRefAssignCopiesOnly) {
    pp::Slot<Payload> slot(Payload{"old"});
    Payload next{"new"};
    Stats::reset();
    slot.assign(next);
    EXPECT_EQ(Stats::copies, 1);
    EXPECT_EQ(Stats::moves, 0);
    EXPECT_EQ(slot.get().bytes, "new");
}

TEST(Part1Sink, RvalueAssignMovesOnly) {
    pp::Slot<Payload> slot(Payload{"old"});
    Payload next{"new"};
    Stats::reset();
    slot.assign(std::move(next));
    EXPECT_EQ(Stats::copies, 0);
    EXPECT_EQ(Stats::moves, 1);
    EXPECT_EQ(slot.get().bytes, "new");
}

TEST(Part1Sink, GetReturnsReferenceWithoutCopyOrMove) {
    pp::Slot<Payload> slot(Payload{"stay"});
    Stats::reset();
    const Payload& view = slot.get();
    EXPECT_EQ(view.bytes, "stay");
    EXPECT_EQ(&slot.get(), &view);
    EXPECT_EQ(Stats::copies, 0);
    EXPECT_EQ(Stats::moves, 0);
}

TEST(Part1Sink, TakeMovesValueOutOfRvalueSlot) {
    pp::Slot<Payload> slot(Payload{"gift"});
    Stats::reset();
    Payload out = std::move(slot).take();
    EXPECT_EQ(out.bytes, "gift");
    EXPECT_EQ(Stats::copies, 0);
    EXPECT_EQ(Stats::moves, 1);
}

// ---------------------------------------------------------------------------
// Part 2
// ---------------------------------------------------------------------------

TEST(Part2Forwarding, MakeOwnedForwardsLvalueAsCopy) {
    Payload p{"abc"};
    Stats::reset();
    auto owned = pp::make_owned<Payload>(p);
    ASSERT_NE(owned, nullptr);
    EXPECT_EQ(owned->bytes, "abc");
    EXPECT_EQ(Stats::copies, 1);
    EXPECT_EQ(Stats::moves, 0);
}

TEST(Part2Forwarding, MakeOwnedForwardsRvalueAsMove) {
    Payload p{"abc"};
    Stats::reset();
    auto owned = pp::make_owned<Payload>(std::move(p));
    ASSERT_NE(owned, nullptr);
    EXPECT_EQ(owned->bytes, "abc");
    EXPECT_EQ(Stats::copies, 0);
    EXPECT_EQ(Stats::moves, 1);
}

TEST(Part2Forwarding, MakeOwnedForwardsSeveralConstructorArguments) {
    auto owned = pp::make_owned<std::string>(3, 'x');
    ASSERT_NE(owned, nullptr);
    EXPECT_EQ(*owned, "xxx");
}

TEST(Part2Forwarding, RegistryEmplaceConstructsInPlaceFromRvalue) {
    pp::Registry<Payload> registry;
    Payload p{"abc"};
    Stats::reset();
    Payload& stored = registry.emplace("a", std::move(p));
    EXPECT_EQ(stored.bytes, "abc");
    EXPECT_EQ(Stats::copies, 0);
    EXPECT_EQ(Stats::moves, 1);
    EXPECT_EQ(registry.size(), 1u);
    ASSERT_NE(registry.find("a"), nullptr);
    EXPECT_EQ(registry.find("a")->bytes, "abc");
}

TEST(Part2Forwarding, RegistryEmplaceCopiesLvalueArgument) {
    pp::Registry<Payload> registry;
    Payload p{"keep"};
    Stats::reset();
    registry.emplace("k", p);
    EXPECT_EQ(Stats::copies, 1);
    EXPECT_EQ(Stats::moves, 0);
    EXPECT_EQ(p.bytes, "keep");
}

TEST(Part2Forwarding, RegistryDuplicateKeyThrowsWithoutConsumingArguments) {
    pp::Registry<Payload> registry;
    registry.emplace("a", Payload{"first"});
    Payload second{"second"};
    Stats::reset();
    EXPECT_THROW(registry.emplace("a", std::move(second)), std::invalid_argument);
    EXPECT_EQ(Stats::moves, 0);
    EXPECT_EQ(second.bytes, "second");
    EXPECT_EQ(registry.find("a")->bytes, "first");
    EXPECT_EQ(registry.size(), 1u);
}

TEST(Part2Forwarding, RegistryFindMissingReturnsNullptr) {
    pp::Registry<std::string> registry;
    registry.emplace("k", 3, 'z');
    EXPECT_EQ(*registry.find("k"), "zzz");
    EXPECT_EQ(registry.find("missing"), nullptr);
}

// ---------------------------------------------------------------------------
// Part 3
// ---------------------------------------------------------------------------

class RecordingTransport final : public pp::Transport {
public:
    mutable std::vector<int> seen_attempts;
    mutable std::vector<std::string> seen_urls;

private:
    pp::Response do_send(const pp::Request& request, int attempts) const override {
        seen_attempts.push_back(attempts);
        seen_urls.push_back(request.url);
        return pp::Response{.status = 200, .attempts = attempts};
    }
};

TEST(Part3Request, NormalizesMethodAndLowercasesHeaderNames) {
    pp::RequestOptions options{
        .headers = {{"Accept", "application/json"}, {"X-Trace-Id", "42"}},
    };
    const pp::Request req = pp::build_request(pp::Method::post, "https://api.example.com/v1", std::move(options));
    EXPECT_EQ(req.method, pp::Method::post);
    EXPECT_EQ(req.url, "https://api.example.com/v1");
    ASSERT_EQ(req.headers.size(), 2u);
    EXPECT_EQ(req.headers[0].name, "accept");
    EXPECT_EQ(req.headers[0].value, "application/json");
    EXPECT_EQ(req.headers[1].name, "x-trace-id");
    EXPECT_EQ(req.headers[1].value, "42");
}

TEST(Part3Request, OmittedOptionsUseDefaults) {
    const pp::Request req = pp::build_request(pp::Method::get, "http://localhost:8080/");
    EXPECT_TRUE(req.headers.empty());
    EXPECT_EQ(req.timeout, 5000ms);
    EXPECT_EQ(req.retries, 0);
    EXPECT_TRUE(req.follow_redirects);
}

TEST(Part3Request, DesignatedInitializersOverrideOnlyNamedFields) {
    const pp::Request req =
        pp::build_request(pp::Method::put, "https://example.com", pp::RequestOptions{.retries = 2});
    EXPECT_EQ(req.retries, 2);
    EXPECT_EQ(req.timeout, 5000ms);
    EXPECT_TRUE(req.follow_redirects);
}

TEST(Part3Request, SchemeCheckIsCaseInsensitiveAndRejectsOthers) {
    EXPECT_NO_THROW(pp::build_request(pp::Method::get, "HTTPS://example.com"));
    EXPECT_THROW(pp::build_request(pp::Method::get, "ftp://example.com"), std::invalid_argument);
    EXPECT_THROW(pp::build_request(pp::Method::get, "example.com/path"), std::invalid_argument);
}

TEST(Part3Request, NegativeRetriesAreRejected) {
    EXPECT_THROW(pp::build_request(pp::Method::get, "https://example.com", pp::RequestOptions{.retries = -1}),
                 std::invalid_argument);
}

TEST(Part3Request, ParseMethodIsCaseInsensitiveAndRejectsUnknown) {
    EXPECT_EQ(pp::parse_method("patch"), pp::Method::patch);
    EXPECT_EQ(pp::parse_method("OPTIONS"), pp::Method::options);
    EXPECT_EQ(pp::method_name(pp::Method::del), "DELETE");
    EXPECT_THROW(pp::parse_method("TRACE"), std::invalid_argument);
}

TEST(Part3Transport, DefaultAttemptsComeFromNonVirtualSendThroughBaseReference) {
    RecordingTransport transport;
    const pp::Transport& base = transport;
    const pp::Request req = pp::build_request(pp::Method::get, "https://example.com/a");
    const pp::Response res = base.send(req);
    ASSERT_EQ(transport.seen_attempts.size(), 1u);
    EXPECT_EQ(transport.seen_attempts[0], 3);
    EXPECT_EQ(res.attempts, 3);
    EXPECT_EQ(res.status, 200);
}

TEST(Part3Transport, ExplicitAttemptsAreForwarded) {
    RecordingTransport transport;
    const pp::Request req = pp::build_request(pp::Method::get, "https://example.com/b");
    transport.send(req, 5);
    transport.send(req, 1);
    EXPECT_THAT(transport.seen_attempts, ::testing::ElementsAre(5, 1));
    EXPECT_THAT(transport.seen_urls, ::testing::ElementsAre("https://example.com/b", "https://example.com/b"));
}

TEST(Part3Transport, ZeroAttemptsRejectedWithoutCallingBackend) {
    RecordingTransport transport;
    const pp::Request req = pp::build_request(pp::Method::get, "https://example.com/c");
    EXPECT_THROW(transport.send(req, 0), std::invalid_argument);
    EXPECT_TRUE(transport.seen_attempts.empty());
}

// ---------------------------------------------------------------------------
// Part 4
// ---------------------------------------------------------------------------

pp::PatchField set_field(std::string value) {
    return pp::PatchField{std::optional<std::string>{std::move(value)}};
}

pp::PatchField clear_field() {
    return pp::PatchField{std::optional<std::string>{}};
}

pp::User base_user() {
    return pp::User{.email = "neo@example.com", .nickname = std::string("neo")};
}

TEST(Part4Patch, EmptyPatchLeavesUserUnchanged) {
    EXPECT_EQ(pp::apply_patch(base_user(), pp::UserPatch{}), base_user());
}

TEST(Part4Patch, NulloptOuterMeansAbsentNotClear) {
    const pp::UserPatch patch{.nickname = std::nullopt};
    const pp::User result = pp::apply_patch(base_user(), patch);
    ASSERT_TRUE(result.nickname.has_value());
    EXPECT_EQ(*result.nickname, "neo");
}

TEST(Part4Patch, EngagedOuterWithEmptyInnerClearsNickname) {
    const pp::UserPatch patch{.nickname = clear_field()};
    const pp::User result = pp::apply_patch(base_user(), patch);
    EXPECT_FALSE(result.nickname.has_value());
    EXPECT_EQ(result.email, "neo@example.com");
}

TEST(Part4Patch, EmailIsTrimmedAndLowercased) {
    const pp::UserPatch patch{.email = set_field("  Trinity@Example.COM ")};
    EXPECT_EQ(pp::apply_patch(base_user(), patch).email, "trinity@example.com");
}

TEST(Part4Patch, MalformedEmailIsRejected) {
    for (const char* bad : {"not-an-email", "a@b", "a b@c.d", "@x.io", "a@@x.io", "a@x."}) {
        const pp::UserPatch patch{.email = set_field(bad)};
        EXPECT_THROW(pp::apply_patch(base_user(), patch), std::invalid_argument) << bad;
    }
}

TEST(Part4Patch, ClearingEmailIsRejected) {
    const pp::UserPatch patch{.email = clear_field()};
    EXPECT_THROW(pp::apply_patch(base_user(), patch), std::invalid_argument);
}

TEST(Part4Patch, NicknameIsTrimmedAndLengthChecked) {
    const pp::UserPatch trimmed{.nickname = set_field("  trinity  ")};
    EXPECT_EQ(*pp::apply_patch(base_user(), trimmed).nickname, "trinity");

    const pp::UserPatch exactly_32{.nickname = set_field(std::string(32, 'n'))};
    EXPECT_EQ(pp::apply_patch(base_user(), exactly_32).nickname->size(), 32u);

    const pp::UserPatch too_long{.nickname = set_field(std::string(33, 'n'))};
    EXPECT_THROW(pp::apply_patch(base_user(), too_long), std::invalid_argument);

    const pp::UserPatch blank{.nickname = set_field("   ")};
    EXPECT_THROW(pp::apply_patch(base_user(), blank), std::invalid_argument);
}

TEST(Part4Patch, CallerObjectIsNeverModified) {
    const pp::User original = base_user();
    pp::User copy = original;
    const pp::UserPatch patch{.email = set_field("new@example.org"), .nickname = clear_field()};
    const pp::User result = pp::apply_patch(copy, patch);
    EXPECT_EQ(result.email, "new@example.org");
    EXPECT_FALSE(result.nickname.has_value());
    EXPECT_EQ(copy, original);
}

// ---------------------------------------------------------------------------
// Part 5
// ---------------------------------------------------------------------------

TEST(Part5Views, SplitKeepsEmptyFields) {
    EXPECT_THAT(pp::split_fields("a,,b", ','), ::testing::ElementsAre("a", "", "b"));
    EXPECT_THAT(pp::split_fields("", ','), ::testing::ElementsAre(""));
    EXPECT_THAT(pp::split_fields("x;", ';'), ::testing::ElementsAre("x", ""));
}

TEST(Part5Views, ReturnedViewsAliasTheInputBuffer) {
    const std::string line = "id,name,email";
    const auto fields = pp::split_fields(line, ',');
    ASSERT_EQ(fields.size(), 3u);
    EXPECT_EQ(fields[0].data(), line.data());
    EXPECT_EQ(fields[1].data(), line.data() + 3);
    EXPECT_EQ(fields[2].data(), line.data() + 8);
}

TEST(Part5Views, JoinAcceptsVectorAndArrayViaSpan) {
    const std::vector<std::string_view> from_vector{"a", "b", "c"};
    const std::array<std::string_view, 3> from_array{"x", "", "z"};
    EXPECT_EQ(pp::join_fields(from_vector, '|'), "a|b|c");
    EXPECT_EQ(pp::join_fields(from_array, ','), "x,,z");
    EXPECT_EQ(pp::join_fields({}, ','), "");
}

TEST(Part5Views, StringLiteralAndStringsRenderQuotedNotAsBool) {
    EXPECT_EQ(pp::format_value("abc"), "\"abc\"");
    EXPECT_EQ(pp::format_value(std::string_view("abc")), "\"abc\"");
    EXPECT_EQ(pp::format_value(std::string("abc")), "\"abc\"");
}

TEST(Part5Views, ScalarOverloadsRenderTheirOwnType) {
    EXPECT_EQ(pp::format_value(true), "true");
    EXPECT_EQ(pp::format_value(false), "false");
    EXPECT_EQ(pp::format_value(std::int64_t{-7}), "-7");
    EXPECT_EQ(pp::format_value(2.5), "2.5");
}

// ---------------------------------------------------------------------------
// Part 6
// ---------------------------------------------------------------------------

TEST(Part6Query, NoColumnsRendersStar) {
    const pp::Sql sql = pp::select("users", pp::SelectOptions{});
    EXPECT_EQ(sql.text, "SELECT * FROM users");
    EXPECT_TRUE(sql.params.empty());
}

TEST(Part6Query, VariadicColumnsAcceptMixedStringTypes) {
    const std::string email = "email";
    const pp::Sql sql = pp::select("users", pp::SelectOptions{}, "id", email, std::string_view("u.name"));
    EXPECT_EQ(sql.text, "SELECT id, email, u.name FROM users");
}

TEST(Part6Query, WhereJoinsWithAndInKeyOrderAndBindsValues) {
    const pp::SelectOptions options{
        .where = {{"active", pp::Value{true}}, {"deleted_at", pp::Value{nullptr}}},
        .order_by = {"-id"},
        .limit = 20,
    };
    const pp::Sql sql = pp::select("users", options, "id", "email");
    EXPECT_EQ(sql.text,
              "SELECT id, email FROM users WHERE active = ? AND deleted_at IS NULL ORDER BY id DESC LIMIT ?");
    EXPECT_EQ(sql.params, (std::vector<pp::Value>{pp::Value{true}, pp::Value{std::int64_t{20}}}));
}

TEST(Part6Query, NullWhereValueRendersIsNullWithoutParameter) {
    const pp::SelectOptions options{.where = {{"owner_id", pp::Value{nullptr}}}};
    const pp::Sql sql = pp::select("tasks", options, "id");
    EXPECT_EQ(sql.text, "SELECT id FROM tasks WHERE owner_id IS NULL");
    EXPECT_TRUE(sql.params.empty());
}

TEST(Part6Query, EmptyWhereAddsNoClause) {
    const pp::SelectOptions options{.where = {}};
    EXPECT_EQ(pp::select("tasks", options, "id").text, "SELECT id FROM tasks");
}

TEST(Part6Query, OrderByHandlesDescendingAndSeveralTerms) {
    const pp::SelectOptions options{.order_by = {"-created", "name"}};
    EXPECT_EQ(pp::select("users", options, "id").text, "SELECT id FROM users ORDER BY created DESC, name");
}

TEST(Part6Query, LimitBindsParameterAndNegativeIsRejected) {
    const pp::Sql sql = pp::select("users", pp::SelectOptions{.limit = 0}, "id");
    EXPECT_EQ(sql.text, "SELECT id FROM users LIMIT ?");
    EXPECT_EQ(sql.params, (std::vector<pp::Value>{pp::Value{std::int64_t{0}}}));
    EXPECT_THROW(pp::select("users", pp::SelectOptions{.limit = -1}, "id"), std::invalid_argument);
}

TEST(Part6Query, InvalidIdentifiersAreRejected) {
    EXPECT_THROW(pp::select("users", pp::SelectOptions{}, "id;DROP"), std::invalid_argument);
    EXPECT_THROW(pp::select("users", pp::SelectOptions{}, "a.b.c"), std::invalid_argument);
    EXPECT_THROW(pp::select("1users", pp::SelectOptions{}, "id"), std::invalid_argument);
    EXPECT_THROW(pp::select("users", pp::SelectOptions{.where = {{"x y", pp::Value{true}}}}, "id"),
                 std::invalid_argument);
    EXPECT_THROW(pp::select("users", pp::SelectOptions{.order_by = {"-"}}, "id"), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Part 7
// ---------------------------------------------------------------------------

TEST(Part7EventBus, ListenersRunInSubscriptionOrderAndPublishCountsThem) {
    pp::EventBus<int> bus;
    std::vector<std::string> log;
    bus.subscribe("tick", [&](const int& n) { log.push_back("first:" + std::to_string(n)); });
    bus.subscribe("tick", [&](const int& n) { log.push_back("second:" + std::to_string(n)); });
    EXPECT_EQ(bus.publish("tick", 4), 2u);
    EXPECT_THAT(log, ::testing::ElementsAre("first:4", "second:4"));
}

TEST(Part7EventBus, PublishWithoutListenersIsANoOp) {
    pp::EventBus<int> bus;
    EXPECT_EQ(bus.publish("nobody", 1), 0u);
}

TEST(Part7EventBus, EventsAreIsolatedByName) {
    pp::EventBus<int> bus;
    int hits = 0;
    bus.subscribe("a", [&](const int&) { ++hits; });
    EXPECT_EQ(bus.publish("b", 1), 0u);
    EXPECT_EQ(hits, 0);
}

TEST(Part7EventBus, UnsubscribeIsTrueOnceThenFalse) {
    pp::EventBus<int> bus;
    int hits = 0;
    const pp::SubscriptionId id = bus.subscribe("e", [&](const int&) { ++hits; });
    EXPECT_TRUE(bus.unsubscribe(id));
    EXPECT_FALSE(bus.unsubscribe(id));
    bus.publish("e", 0);
    EXPECT_EQ(hits, 0);
}

TEST(Part7EventBus, OnceListenerFiresOnceEvenWhenItRepublishes) {
    pp::EventBus<int> bus;
    int hits = 0;
    bus.subscribe(
        "e",
        [&](const int& depth) {
            ++hits;
            if (depth < 3) bus.publish("e", depth + 1);
        },
        pp::SubscribeOptions{.once = true});
    bus.publish("e", 0);
    EXPECT_EQ(hits, 1);
}

TEST(Part7EventBus, ThrowingOnceListenerIsStillRemoved) {
    pp::EventBus<int> bus;
    int hits = 0;
    bus.subscribe(
        "e",
        [&](const int&) {
            ++hits;
            throw std::runtime_error("boom");
        },
        pp::SubscribeOptions{.once = true});
    EXPECT_THROW(bus.publish("e", 0), std::runtime_error);
    EXPECT_NO_THROW(bus.publish("e", 0));
    EXPECT_EQ(hits, 1);
}

TEST(Part7EventBus, UnsubscribeDuringPublishDoesNotSkipScheduledListeners) {
    pp::EventBus<int> bus;
    std::vector<std::string> log;
    pp::SubscriptionId victim = 0;
    bus.subscribe("e", [&](const int&) {
        log.push_back("killer");
        bus.unsubscribe(victim);
    });
    victim = bus.subscribe("e", [&](const int&) { log.push_back("victim"); });
    EXPECT_EQ(bus.publish("e", 0), 2u);
    EXPECT_THAT(log, ::testing::ElementsAre("killer", "victim"));

    log.clear();
    bus.publish("e", 0);
    EXPECT_THAT(log, ::testing::ElementsAre("killer"));
}

TEST(Part7EventBus, ListenerExceptionStopsPublishImmediately) {
    pp::EventBus<int> bus;
    int later_hits = 0;
    bus.subscribe("e", [](const int&) { throw std::runtime_error("fail"); });
    bus.subscribe("e", [&](const int&) { ++later_hits; });
    EXPECT_THROW(bus.publish("e", 0), std::runtime_error);
    EXPECT_EQ(later_hits, 0);
}

TEST(Part7EventBus, EmptyListenerIsRejected) {
    pp::EventBus<int> bus;
    EXPECT_THROW(bus.subscribe("e", nullptr), std::invalid_argument);
}

TEST(Part7EventBus, PublishCopiesLvalueOnceAndListenersBorrow) {
    pp::EventBus<Payload> bus;
    int hits = 0;
    bus.subscribe("e", [&](const Payload&) { ++hits; });
    bus.subscribe("e", [&](const Payload&) { ++hits; });
    Payload p{"abc"};
    Stats::reset();
    bus.publish("e", p);
    EXPECT_EQ(hits, 2);
    EXPECT_EQ(Stats::copies, 1);
    EXPECT_EQ(Stats::moves, 0);
}

TEST(Part7EventBus, PublishMovesRvalueOnceAndListenersBorrow) {
    pp::EventBus<Payload> bus;
    bus.subscribe("e", [](const Payload&) {});
    bus.subscribe("e", [](const Payload&) {});
    Payload p{"abc"};
    Stats::reset();
    bus.publish("e", std::move(p));
    EXPECT_EQ(Stats::copies, 0);
    EXPECT_EQ(Stats::moves, 1);
}

TEST(Part7EventBus, ByValueListenerPaysOneCopyPerCall) {
    pp::EventBus<Payload> bus;
    bus.subscribe("e", [](Payload) {});
    bus.subscribe("e", [](Payload) {});
    Payload p{"abc"};
    Stats::reset();
    bus.publish("e", p);
    EXPECT_EQ(Stats::copies, 3);  // one sink in publish + one copy per by-value listener
}

}  // namespace
