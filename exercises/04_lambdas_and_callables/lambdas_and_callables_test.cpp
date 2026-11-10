#include "lambdas_and_callables.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <expected>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

using namespace cppref::lambdas;
using namespace std::chrono_literals;
using ::testing::_;
using ::testing::InSequence;

// ---- compile-time facts (stub-safe: they depend only on declarations) ----

static_assert(sizeof(FunctionRef<int(int)>) == 2 * sizeof(void*));

namespace {
using RvalueClosure = decltype([](int x) { return x; });
static_assert(!std::is_constructible_v<FunctionRef<int(int)>, RvalueClosure>,
              "FunctionRef must not bind to temporaries");
}  // namespace

namespace {

TimePoint at_ms(std::chrono::milliseconds ms) { return TimePoint{ms}; }

// Clock that replays `ticks` in order and counts reads in `reads`.
auto scripted_clock(std::vector<TimePoint> ticks, int& reads) {
    return [ticks = std::move(ticks), &reads, i = std::size_t{0}]() mutable -> TimePoint {
        ++reads;
        return ticks.at(i++);
    };
}

using Result = std::expected<int, std::error_code>;

std::error_code ec(std::errc e) { return std::make_error_code(e); }
Result ok(int v) { return v; }
Result fail(std::errc e) { return Result(std::unexpect, ec(e)); }

}  // namespace

// ===== Part 1: closures and composition =====

TEST(Part1Closures, MultiplierCapturesFactorByValue) {
    int factor = 3;
    auto times3 = make_multiplier(factor);
    factor = 100;
    EXPECT_EQ(times3(4), 12);
}

TEST(Part1Closures, MultiplierIsCallableRepeatedly) {
    auto negate_double = make_multiplier(-2);
    EXPECT_EQ(negate_double(5), -10);
    EXPECT_EQ(negate_double(0), 0);
}

TEST(Part1Closures, AccumulatorKeepsRunningTotal) {
    auto acc = make_accumulator();
    EXPECT_EQ(acc(5), 5);
    EXPECT_EQ(acc(-2), 3);
    EXPECT_EQ(acc(10), 13);
}

TEST(Part1Closures, AccumulatorInstancesAreIndependentAndCopiesCarryState) {
    auto a = make_accumulator();
    a(4);
    auto b = make_accumulator();
    EXPECT_EQ(b(1), 1);
    auto copy = a;
    EXPECT_EQ(copy(1), 5);
    EXPECT_EQ(a(1), 5);
}

TEST(Part1Closures, AddersCaptureTheirIndex) {
    auto adders = make_adders(3);
    ASSERT_EQ(adders.size(), 3u);
    EXPECT_EQ(adders[0](10), 10);
    EXPECT_EQ(adders[1](10), 11);
    EXPECT_EQ(adders[2](10), 12);
}

TEST(Part1Closures, AddersZeroCountIsEmpty) {
    EXPECT_TRUE(make_adders(0).empty());
}

TEST(Part1Closures, AddersRejectNegativeCount) {
    EXPECT_THROW(make_adders(-1), std::invalid_argument);
}

TEST(Part1Closures, ComposeAppliesRightmostFirst) {
    auto f = compose([](int x) { return x * 2; }, [](int x) { return x + 1; });
    EXPECT_EQ(f(3), 8);  // (3 + 1) * 2
}

TEST(Part1Closures, ComposeOfOneFunctionIsThatFunction) {
    auto f = compose([](int x) { return x - 1; });
    EXPECT_EQ(f(10), 9);
}

TEST(Part1Closures, ComposeWithNoFunctionsIsIdentity) {
    auto id = compose();
    EXPECT_EQ(id(7), 7);
}

// ===== Part 2: forwarding decorator =====

TEST(Part2Timed, ReportsLabelAndElapsedTime) {
    int reads = 0;
    std::vector<Timing> seen;
    TimingSink sink = [&seen](const Timing& t) { seen.push_back(t); };
    auto parse = timed("parse", [](int x) { return x + 1; }, sink,
                       scripted_clock({at_ms(10ms), at_ms(35ms)}, reads));
    EXPECT_EQ(parse(41), 42);
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_EQ(seen[0].label, "parse");
    EXPECT_EQ(seen[0].elapsed, std::chrono::nanoseconds{25ms});
    EXPECT_FALSE(seen[0].threw);
}

TEST(Part2Timed, ReadsClockExactlyTwicePerCall) {
    int reads = 0;
    TimingSink sink = [](const Timing&) {};
    auto f = timed("f", [](int x) { return x; }, sink,
                   scripted_clock({at_ms(0ms), at_ms(1ms), at_ms(2ms), at_ms(3ms)}, reads));
    f(1);
    EXPECT_EQ(reads, 2);
    f(2);
    EXPECT_EQ(reads, 4);
}

TEST(Part2Timed, ReportsThrowThenRethrows) {
    int reads = 0;
    std::vector<Timing> seen;
    TimingSink sink = [&seen](const Timing& t) { seen.push_back(t); };
    auto f = timed("boom", []() -> int { throw std::runtime_error("boom"); }, sink,
                   scripted_clock({at_ms(0ms), at_ms(5ms)}, reads));
    EXPECT_THROW(f(), std::runtime_error);
    ASSERT_EQ(seen.size(), 1u);
    EXPECT_TRUE(seen[0].threw);
}

TEST(Part2Timed, VoidCallableIsSupported) {
    int reads = 0;
    int hits = 0;
    std::vector<Timing> seen;
    TimingSink sink = [&seen](const Timing& t) { seen.push_back(t); };
    auto tick = timed("tick", [&hits] { ++hits; }, sink, scripted_clock({at_ms(0ms), at_ms(1ms)}, reads));
    tick();
    EXPECT_EQ(hits, 1);
    EXPECT_EQ(seen.size(), 1u);
}

TEST(Part2Timed, PreservesReferenceReturn) {
    int reads = 0;
    int target = 1;
    TimingSink sink = [](const Timing&) {};
    auto get = timed("ref", [&target]() -> int& { return target; }, sink,
                     scripted_clock({at_ms(0ms), at_ms(1ms)}, reads));
    get() = 9;
    EXPECT_EQ(target, 9);
}

TEST(Part2Timed, PreservesMoveOnlyArgumentsAndResults) {
    int reads = 0;
    TimingSink sink = [](const Timing&) {};
    auto pass = timed("make", [](std::unique_ptr<int> p) { return p; }, sink,
                      scripted_clock({at_ms(0ms), at_ms(1ms)}, reads));
    auto r = pass(std::make_unique<int>(7));
    ASSERT_TRUE(r);
    EXPECT_EQ(*r, 7);
}

// ===== Part 3: retry with backoff =====

TEST(Part3Retry, SuccessOnFirstAttemptDoesNotSleep) {
    testing::MockFunction<void(std::chrono::milliseconds)> sleep;
    EXPECT_CALL(sleep, Call(_)).Times(0);
    int calls = 0;
    auto fn = [&calls]() -> Result {
        ++calls;
        return ok(42);
    };
    auto r = retry(fn, RetryPolicy{}, sleep.AsStdFunction());
    auto out = r();
    ASSERT_TRUE(out);
    EXPECT_EQ(*out, 42);
    EXPECT_EQ(calls, 1);
}

TEST(Part3Retry, RetriesUntilSuccess) {
    testing::MockFunction<void(std::chrono::milliseconds)> sleep;
    int calls = 0;
    auto fn = [&calls]() -> Result { return ++calls < 3 ? fail(std::errc::io_error) : ok(42); };
    auto r = retry(fn, RetryPolicy{}, sleep.AsStdFunction());
    {
        InSequence seq;
        EXPECT_CALL(sleep, Call(100ms));
        EXPECT_CALL(sleep, Call(200ms));
    }
    auto out = r();
    ASSERT_TRUE(out);
    EXPECT_EQ(*out, 42);
    EXPECT_EQ(calls, 3);
}

TEST(Part3Retry, BacksOffExponentiallyWithoutSleepingAfterLastAttempt) {
    testing::MockFunction<void(std::chrono::milliseconds)> sleep;
    int calls = 0;
    auto fn = [&calls]() -> Result {
        ++calls;
        return fail(std::errc::io_error);
    };
    RetryPolicy policy;
    policy.max_attempts = 4;
    auto r = retry(fn, policy, sleep.AsStdFunction());
    {
        InSequence seq;
        EXPECT_CALL(sleep, Call(100ms));
        EXPECT_CALL(sleep, Call(200ms));
        EXPECT_CALL(sleep, Call(400ms));
    }
    auto out = r();
    EXPECT_FALSE(out);
    EXPECT_EQ(calls, 4);
}

TEST(Part3Retry, DelayIsCappedAtMaxDelay) {
    testing::MockFunction<void(std::chrono::milliseconds)> sleep;
    auto fn = []() -> Result { return fail(std::errc::io_error); };
    RetryPolicy policy;
    policy.max_attempts = 4;
    policy.base_delay = 100ms;
    policy.multiplier = 10.0;
    policy.max_delay = 500ms;
    auto r = retry(fn, policy, sleep.AsStdFunction());
    {
        InSequence seq;
        EXPECT_CALL(sleep, Call(100ms));
        EXPECT_CALL(sleep, Call(500ms));
        EXPECT_CALL(sleep, Call(500ms));
    }
    (void)r();
}

TEST(Part3Retry, ExhaustedRetriesReturnLastError) {
    testing::MockFunction<void(std::chrono::milliseconds)> sleep;
    EXPECT_CALL(sleep, Call(_)).Times(2);
    int calls = 0;
    auto fn = [&calls]() -> Result {
        ++calls;
        const std::errc e = calls == 1   ? std::errc::io_error
                            : calls == 2 ? std::errc::timed_out
                                         : std::errc::resource_unavailable_try_again;
        return fail(e);
    };
    auto r = retry(fn, RetryPolicy{}, sleep.AsStdFunction());
    auto out = r();
    ASSERT_FALSE(out);
    EXPECT_EQ(out.error(), ec(std::errc::resource_unavailable_try_again));
    EXPECT_EQ(calls, 3);
}

TEST(Part3Retry, NonRetryableErrorReturnsImmediately) {
    testing::MockFunction<void(std::chrono::milliseconds)> sleep;
    EXPECT_CALL(sleep, Call(_)).Times(0);
    int calls = 0;
    auto fn = [&calls]() -> Result {
        ++calls;
        return fail(std::errc::io_error);
    };
    RetryPolicy policy;
    policy.retryable = [](const std::error_code& e) {
        return e == std::make_error_code(std::errc::resource_unavailable_try_again);
    };
    auto r = retry(fn, policy, sleep.AsStdFunction());
    auto out = r();
    ASSERT_FALSE(out);
    EXPECT_EQ(out.error(), ec(std::errc::io_error));
    EXPECT_EQ(calls, 1);
}

TEST(Part3Retry, ArgumentsArePassedAsLvaluesOnEveryAttempt) {
    testing::MockFunction<void(std::chrono::milliseconds)> sleep;
    EXPECT_CALL(sleep, Call(_)).Times(2);
    auto fn = [](int& n) -> Result {
        ++n;
        return n < 3 ? fail(std::errc::io_error) : ok(n);
    };
    auto r = retry(fn, RetryPolicy{}, sleep.AsStdFunction());
    int n = 0;
    auto out = r(n);
    ASSERT_TRUE(out);
    EXPECT_EQ(*out, 3);
    EXPECT_EQ(n, 3);
}

TEST(Part3Retry, InvalidPolicyThrowsAtRetryCall) {
    testing::MockFunction<void(std::chrono::milliseconds)> sleep;
    auto fn = []() -> Result { return ok(1); };
    RetryPolicy policy;
    policy.max_attempts = 0;
    EXPECT_THROW(retry(fn, policy, sleep.AsStdFunction()), std::invalid_argument);
}

// ===== Part 4: type erasure without ownership =====

namespace {
int twice(int x) { return 2 * x; }
}  // namespace

TEST(Part4TypeErasure, FunctionRefInvokesCapturingLambda) {
    int offset = 10;
    auto add = [offset](int x) { return x + offset; };
    FunctionRef<int(int)> ref(add);
    EXPECT_EQ(ref(5), 15);
}

TEST(Part4TypeErasure, FunctionRefSeesCallsThroughCapturedReference) {
    int calls = 0;
    auto counting = [&calls](int x) {
        ++calls;
        return x;
    };
    FunctionRef<int(int)> ref(counting);
    ref(1);
    ref(2);
    EXPECT_EQ(calls, 2);
}

TEST(Part4TypeErasure, FunctionRefBindsFunctionPointer) {
    int (*fp)(int) = &twice;
    FunctionRef<int(int)> ref(fp);
    EXPECT_EQ(ref(4), 8);
}

TEST(Part4TypeErasure, SumWithAppliesCallableToEveryElement) {
    std::array<int, 3> xs{1, 2, 3};
    auto square = [](int x) { return x * x; };
    EXPECT_EQ(sum_with(xs, square), 14);
}

TEST(Part4TypeErasure, MakeTaggerOwnsTagAndFormatsBody) {
    auto warn = make_tagger("warn");
    EXPECT_EQ(warn("disk"), "[warn] disk");
    EXPECT_EQ(tag_line("info", "ok"), "[info] ok");
}

// ===== Part 5: stateful callables =====

TEST(Part5State, CallCounterCountsAndForwardsArguments) {
    auto counted = CallCounter([](int a, int b) { return a + b; });
    EXPECT_EQ(counted(2, 3), 5);
    EXPECT_EQ(counted.count(), 1u);
    counted(1, 1);
    EXPECT_EQ(counted.count(), 2u);
}

TEST(Part5State, CallCounterResetZeroesCount) {
    auto counted = CallCounter([] {});
    counted();
    counted();
    counted.reset();
    EXPECT_EQ(counted.count(), 0u);
}

TEST(Part5State, CallCounterCopiesCarryTheirOwnCount) {
    auto counted = CallCounter([] {});
    counted();
    counted();
    auto copy = counted;
    EXPECT_EQ(copy.count(), 2u);
    copy();
    EXPECT_EQ(copy.count(), 3u);
    EXPECT_EQ(counted.count(), 2u);
}

TEST(Part5State, LimiterAllowsUpToMaxThenRefuses) {
    int t = 0;
    NowFn now = [&t] { return at_ms(std::chrono::milliseconds{t}); };
    SlidingWindowLimiter limiter(2, 1000ms, now);
    EXPECT_TRUE(limiter.try_acquire());
    EXPECT_TRUE(limiter.try_acquire());
    EXPECT_FALSE(limiter.try_acquire());
}

TEST(Part5State, LimiterRetryAfterIsTimeUntilOldestExpires) {
    int t = 0;
    NowFn now = [&t] { return at_ms(std::chrono::milliseconds{t}); };
    SlidingWindowLimiter limiter(2, 1000ms, now);
    t = 0;
    ASSERT_TRUE(limiter.try_acquire());
    t = 300;
    ASSERT_TRUE(limiter.try_acquire());
    t = 400;
    auto refused = limiter.try_acquire();
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error(), 600ms);
}

TEST(Part5State, LimiterWindowExpiresExactlyAtPeriod) {
    int t = 0;
    NowFn now = [&t] { return at_ms(std::chrono::milliseconds{t}); };
    SlidingWindowLimiter limiter(1, 1000ms, now);
    ASSERT_TRUE(limiter.try_acquire());
    t = 999;
    EXPECT_FALSE(limiter.try_acquire());
    t = 1000;
    EXPECT_TRUE(limiter.try_acquire());
}

TEST(Part5State, LimiterRejectsInvalidConfiguration) {
    NowFn now = [] { return TimePoint{}; };
    EXPECT_THROW(SlidingWindowLimiter(0, 1000ms, now), std::invalid_argument);
    EXPECT_THROW(SlidingWindowLimiter(1, 0ms, now), std::invalid_argument);
}

TEST(Part5State, RateLimitedForwardsWhenAdmitted) {
    NowFn now = [] { return TimePoint{}; };
    RateLimited limited([](int x) { return x * 2; }, SlidingWindowLimiter(1, 1000ms, now));
    auto first = limited(21);
    ASSERT_TRUE(first);
    EXPECT_EQ(*first, 42);
    EXPECT_FALSE(limited(1));
}

TEST(Part5State, RateLimitedRefusesWithRetryAfter) {
    int t = 0;
    NowFn now = [&t] { return at_ms(std::chrono::milliseconds{t}); };
    RateLimited limited([](int x) { return x; }, SlidingWindowLimiter(1, 1000ms, now));
    ASSERT_TRUE(limited(1));
    t = 250;
    auto refused = limited(2);
    ASSERT_FALSE(refused);
    EXPECT_EQ(refused.error(), 750ms);
}

TEST(Part5State, MemoizeCachesResults) {
    int t = 0;
    NowFn now = [&t] { return at_ms(std::chrono::milliseconds{t}); };
    int inner_calls = 0;
    auto memo = memoize_ttl<int(int)>(
        [&inner_calls](int x) {
            ++inner_calls;
            return x * x;
        },
        1000ms, now);
    EXPECT_EQ(memo(3), 9);
    EXPECT_EQ(memo(3), 9);
    EXPECT_EQ(inner_calls, 1);
    EXPECT_EQ(memo.hits(), 1u);
    EXPECT_EQ(memo.misses(), 1u);
    EXPECT_EQ(memo.size(), 1u);
}

TEST(Part5State, MemoizeDistinctArgumentsGetDistinctEntries) {
    NowFn now = [] { return TimePoint{}; };
    auto memo = memoize_ttl<int(int)>([](int x) { return x + 1; }, 1000ms, now);
    EXPECT_EQ(memo(2), 3);
    EXPECT_EQ(memo(3), 4);
    EXPECT_EQ(memo.misses(), 2u);
    EXPECT_EQ(memo.size(), 2u);
}

TEST(Part5State, MemoizeEntriesExpireAfterTtl) {
    int t = 0;
    NowFn now = [&t] { return at_ms(std::chrono::milliseconds{t}); };
    int inner_calls = 0;
    auto memo = memoize_ttl<int(int)>(
        [&inner_calls](int x) {
            ++inner_calls;
            return x;
        },
        1000ms, now);
    t = 0;
    EXPECT_EQ(memo(4), 4);
    t = 999;
    EXPECT_EQ(memo(4), 4);
    EXPECT_EQ(inner_calls, 1);
    t = 1000;
    EXPECT_EQ(memo(4), 4);
    EXPECT_EQ(inner_calls, 2);
}

TEST(Part5State, MemoizeClearDropsEntriesButKeepsCounters) {
    NowFn now = [] { return TimePoint{}; };
    int inner_calls = 0;
    auto memo = memoize_ttl<int(int)>(
        [&inner_calls](int x) {
            ++inner_calls;
            return x;
        },
        1000ms, now);
    memo(5);
    memo.clear();
    EXPECT_EQ(memo.size(), 0u);
    memo(5);
    EXPECT_EQ(inner_calls, 2);
    EXPECT_EQ(memo.misses(), 2u);
}

TEST(Part5State, MemoizeKeysUseDecayedArgumentTypes) {
    NowFn now = [] { return TimePoint{}; };
    int calls = 0;
    auto memo = memoize_ttl<std::size_t(std::string)>(
        [&calls](std::string s) {
            ++calls;
            return s.size();
        },
        1000ms, now);
    EXPECT_EQ(memo(std::string{"abc"}), 3u);
    EXPECT_EQ(memo("abc"), 3u);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(memo.hits(), 1u);
}

// ===== Part 6: overloads, variants, recursion, constexpr lambdas =====

TEST(Part6Overloads, OverloadedPicksMostSpecificOverload) {
    auto f = overloaded{[](int) { return std::string("int"); },
                        [](double) { return std::string("double"); },
                        [](const std::string& s) { return "string:" + s; }};
    EXPECT_EQ(f(3), "int");
    EXPECT_EQ(f(2.5), "double");
    EXPECT_EQ(f(std::string("x")), "string:x");
}

TEST(Part6Overloads, DescribeCoversEveryAlternative) {
    EXPECT_EQ(describe(Event{Connected{"10.0.0.1"}}), "connected 10.0.0.1");
    EXPECT_EQ(describe(Event{Message{"hi"}}), "message hi");
    EXPECT_EQ(describe(Event{Disconnected{1000}}), "disconnected 1000");
}

TEST(Part6Overloads, CountNodesRecursesThroughChildren) {
    Node root{"root",
              {Node{"a", {Node{"a1", {}}, Node{"a2", {}}}},
               Node{"b", {}}}};
    EXPECT_EQ(count_nodes(root), 5u);
}

TEST(Part6Overloads, RenderTreeIndentsByDepth) {
    Node root{"root", {Node{"a", {Node{"a1", {}}}}, Node{"b", {}}}};
    EXPECT_EQ(render_tree(root), "root\n  a\n    a1\n  b\n");
}

TEST(Part6Overloads, IsValidPortChecksRange) {
    EXPECT_FALSE(is_valid_port(0));
    EXPECT_TRUE(is_valid_port(1));
    EXPECT_TRUE(is_valid_port(65535));
    EXPECT_FALSE(is_valid_port(65536));
    EXPECT_FALSE(is_valid_port(-1));
}

// ===== Part 7: dispatch and middleware =====

namespace {
CommandFn sum_command() {
    return [](std::span<const std::string> args) {
        int total = 0;
        for (const auto& a : args) total += std::stoi(a);
        return total;
    };
}
}  // namespace

TEST(Part7Dispatch, RegistryRunsRegisteredCommandWithArgs) {
    CommandRegistry reg;
    reg.add("sum", sum_command());
    EXPECT_EQ(reg.size(), 1u);
    const std::vector<std::string> args{"3", "4"};
    EXPECT_EQ(reg.run("sum", args), 7);
}

TEST(Part7Dispatch, RegistryUnknownCommandThrows) {
    CommandRegistry reg;
    const std::vector<std::string> none;
    EXPECT_THROW(reg.run("nope", none), UnknownCommand);
}

TEST(Part7Dispatch, RegistryRejectsDuplicateNames) {
    CommandRegistry reg;
    reg.add("sum", sum_command());
    EXPECT_THROW(reg.add("sum", sum_command()), std::invalid_argument);
}

TEST(Part7Dispatch, RegistryRejectsEmptyName) {
    CommandRegistry reg;
    EXPECT_THROW(reg.add("", sum_command()), std::invalid_argument);
}

TEST(Part7Dispatch, RegistryNamesAreSorted) {
    CommandRegistry reg;
    reg.add("zeta", sum_command());
    reg.add("alpha", sum_command());
    reg.add("mid", sum_command());
    EXPECT_EQ(reg.names(), (std::vector<std::string>{"alpha", "mid", "zeta"}));
}

namespace {
Middleware tag_middleware(std::vector<std::string>& log, std::string name) {
    return [&log, name](Handler next) -> Handler {
        return [&log, name, next](const Request& req) -> Response {
            log.push_back(name + "-in");
            Response res = next(req);
            log.push_back(name + "-out");
            return res;
        };
    };
}
}  // namespace

TEST(Part7Dispatch, ChainRunsOuterMiddlewareFirst) {
    std::vector<std::string> log;
    Handler terminal = [&log](const Request&) -> Response {
        log.push_back("handler");
        return Response{};
    };
    auto chain = build_chain(terminal, {tag_middleware(log, "a"), tag_middleware(log, "b")});
    (void)chain(Request{"GET", "/", {}});
    EXPECT_EQ(log, (std::vector<std::string>{"a-in", "b-in", "handler", "b-out", "a-out"}));
}

TEST(Part7Dispatch, ChainWithNoMiddlewareIsTerminal) {
    Handler terminal = [](const Request&) { return Response{201, "x"}; };
    auto chain = build_chain(terminal, {});
    EXPECT_EQ(chain(Request{"GET", "/", {}}).status, 201);
}

TEST(Part7Dispatch, RecoverErrorsTurnsExceptionInto500) {
    Handler terminal = [](const Request&) -> Response { throw std::runtime_error("db down"); };
    auto chain = build_chain(terminal, {recover_errors()});
    Response res = chain(Request{"GET", "/", {}});
    EXPECT_EQ(res.status, 500);
    EXPECT_EQ(res.body, "db down");
}

TEST(Part7Dispatch, RecoverErrorsPassesNormalResponsesThrough) {
    Handler terminal = [](const Request&) { return Response{204, ""}; };
    auto chain = build_chain(terminal, {recover_errors()});
    EXPECT_EQ(chain(Request{"GET", "/", {}}).status, 204);
}

TEST(Part7Dispatch, RequireHeaderRejectsMissingWithoutCallingNext) {
    int calls = 0;
    Handler terminal = [&calls](const Request&) {
        ++calls;
        return Response{};
    };
    auto chain = build_chain(terminal, {require_header("X-Token")});
    Response res = chain(Request{"GET", "/", {}});
    EXPECT_EQ(res.status, 400);
    EXPECT_EQ(res.body, "missing header: X-Token");
    EXPECT_EQ(calls, 0);
}

TEST(Part7Dispatch, RequireHeaderPassesWhenPresent) {
    int calls = 0;
    Handler terminal = [&calls](const Request&) {
        ++calls;
        return Response{};
    };
    auto chain = build_chain(terminal, {require_header("X-Token")});
    Request req{"GET", "/", {{"X-Token", "abc"}}};
    EXPECT_EQ(chain(req).status, 200);
    EXPECT_EQ(calls, 1);
}

TEST(Part7Dispatch, AccessLogRecordsEachResponse) {
    testing::MockFunction<void(std::string_view)> sink;
    EXPECT_CALL(sink, Call(std::string_view{"GET /items 200"}));
    Handler terminal = [](const Request&) { return Response{}; };
    auto chain = build_chain(terminal, {access_log(sink.AsStdFunction())});
    (void)chain(Request{"GET", "/items", {}});
}
