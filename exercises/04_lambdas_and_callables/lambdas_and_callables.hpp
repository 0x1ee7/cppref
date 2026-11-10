// Exercise 04: lambdas and callables (STUB).
//
// Every definition below either is a placeholder that calls not_implemented()
// (which throws std::logic_error) or returns a type-correct dummy. Replace them.
// Public signatures, names and namespace are part of the contract: keep them.
#pragma once

#include <chrono>
#include <concepts>
#include <cstddef>
#include <deque>
#include <expected>
#include <functional>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace cppref::lambdas {

/// Throws std::logic_error("not implemented: <name>"). Used by the stubs.
[[noreturn]] void not_implemented(std::string_view name);

using TimePoint = std::chrono::steady_clock::time_point;
/// A "now" source. Injected so that tests can control time.
using NowFn = std::function<TimePoint()>;

// ===== Part 1: closures and composition =====

/// Returns a callable `x -> x * factor`. `factor` is captured by value.
std::function<int(int)> make_multiplier(int factor);

/// Returns a callable that adds its argument to a private running total
/// (starts at 0) and returns the new total. Each call to make_accumulator()
/// produces an independent total.
std::function<int(int)> make_accumulator();

/// Returns `n` callables; element `i` computes `x + i`.
/// Throws std::invalid_argument if n < 0.
std::vector<std::function<int(int)>> make_adders(int n);

/// compose(f1, f2, ..., fn)(x) == f1(f2(...fn(x))). Rightmost runs first.
/// compose() with no arguments is the identity. Every stored callable must be
/// invocable through a const reference.
template <class... Fs>
auto compose([[maybe_unused]] Fs... fs) {
    return [](auto&&...) -> int { not_implemented("compose"); };
}

// ===== Part 2: forwarding decorator =====

/// One measurement reported to a TimingSink.
struct Timing {
    std::string label;
    std::chrono::nanoseconds elapsed{};
    bool threw = false;
};

using TimingSink = std::function<void(const Timing&)>;

/// Default clock: wraps std::chrono::steady_clock::now().
struct SteadyClock {
    TimePoint operator()() const;
};

/// Forwarding decorator. Holds the callable by value.
template <class Fn, class Clock = SteadyClock>
class Timed {
public:
    Timed(std::string label, Fn fn, TimingSink sink, Clock clock)
        : label_(std::move(label)), fn_(std::move(fn)), sink_(std::move(sink)), clock_(std::move(clock)) {
        not_implemented("Timed");
    }

    /// Forwards args (perfectly), returns exactly what fn returns (void,
    /// references and move-only values preserved). Reads the clock before and
    /// after the call and reports one Timing to the sink, even if fn throws
    /// (then rethrows the exception).
    template <class... Args>
    std::invoke_result_t<Fn&, Args...> operator()(Args&&... args) {
        not_implemented("Timed::operator()");
    }

private:
    std::string label_;
    Fn fn_;
    TimingSink sink_;
    Clock clock_;
};

/// Factory: deduces Fn so lambdas can be wrapped directly.
template <class Fn, class Clock = SteadyClock>
Timed<Fn, Clock> timed(std::string label, Fn fn, TimingSink sink, Clock clock = Clock{}) {
    return Timed<Fn, Clock>(std::move(label), std::move(fn), std::move(sink), std::move(clock));
}

// ===== Part 3: retry with backoff =====

/// Retry configuration. Delay before retry k (1-based) is
/// min(base_delay * multiplier^(k-1), max_delay). No delay after the last attempt.
struct RetryPolicy {
    int max_attempts = 3;
    std::chrono::milliseconds base_delay{100};
    double multiplier = 2.0;
    std::chrono::milliseconds max_delay{5000};
    /// Decides whether a given error is worth another attempt.
    std::function<bool(const std::error_code&)> retryable = [](const std::error_code&) { return true; };
};

using SleepFn = std::function<void(std::chrono::milliseconds)>;

/// Throws std::invalid_argument if the policy is inconsistent:
/// max_attempts < 1, base_delay < 0, max_delay < base_delay, multiplier < 1.0.
void validate_policy(const RetryPolicy& policy);

/// Callable returned by retry(). `Fn` returns std::expected<T, std::error_code>.
/// Arguments are passed to `Fn` as lvalues on every attempt.
template <class Fn>
class Retrying {
public:
    Retrying(Fn fn, RetryPolicy policy, SleepFn sleep)
        : fn_(std::move(fn)), policy_(std::move(policy)), sleep_(std::move(sleep)) {
        not_implemented("Retrying");
    }

    /// Calls fn until it succeeds, the error is not retryable, or attempts run out.
    /// Returns the last result.
    template <class... Args>
    std::invoke_result_t<Fn&, Args&...> operator()(Args&&... args) {
        not_implemented("Retrying::operator()");
    }

private:
    Fn fn_;
    RetryPolicy policy_;
    SleepFn sleep_;
};

/// Validates the policy (see validate_policy) and wraps fn.
template <class Fn>
Retrying<Fn> retry(Fn fn, RetryPolicy policy, SleepFn sleep) {
    validate_policy(policy);
    return Retrying<Fn>(std::move(fn), std::move(policy), std::move(sleep));
}

// ===== Part 4: type erasure without ownership =====

template <class Sig>
class FunctionRef;

/// Non-owning reference to any callable with signature R(Args...).
/// Binds only to lvalues (so a temporary lambda cannot dangle). Two pointers
/// in size. The referenced callable must outlive the FunctionRef.
template <class R, class... Args>
class FunctionRef<R(Args...)> {
public:
    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, FunctionRef>) && std::is_invocable_r_v<R, F&, Args...>
    FunctionRef(F& f) {
        not_implemented("FunctionRef");
    }

    R operator()(Args... args) const {
        not_implemented("FunctionRef::operator()");
    }

private:
    void* obj_ = nullptr;
    R (*call_)(void*, Args...) = nullptr;
};

/// Sums f(x) over xs.
int sum_with(std::span<const int> xs, FunctionRef<int(int)> f);

/// Formats "[tag] body".
std::string tag_line(std::string_view tag, std::string_view body);

/// Returns a callable `body -> tag_line(tag, body)` that owns its tag.
std::function<std::string(std::string_view)> make_tagger(std::string tag);

// ===== Part 5: stateful callables =====

/// Wraps F and counts calls. Copies carry their count.
template <class F>
class CallCounter {
public:
    explicit CallCounter(F fn) : fn_(std::move(fn)) {
        not_implemented("CallCounter");
    }

    /// Forwards to fn and increments the count.
    template <class... Args>
    std::invoke_result_t<F&, Args...> operator()(Args&&... args) {
        not_implemented("CallCounter::operator()");
    }

    std::size_t count() const { return count_; }
    void reset() { count_ = 0; }

private:
    F fn_;
    std::size_t count_ = 0;
};

/// Admits at most `max_calls` acquisitions in any window of length `period`.
/// An acquisition at time t stops counting once `t + period <= now`.
class SlidingWindowLimiter {
public:
    /// Throws std::invalid_argument if max_calls == 0 or period <= 0.
    SlidingWindowLimiter(std::size_t max_calls, std::chrono::milliseconds period, NowFn now);

    /// On success returns {}. When the window is full, returns the time until
    /// the oldest in-window acquisition expires.
    std::expected<void, std::chrono::milliseconds> try_acquire();

private:
    std::size_t max_calls_;
    std::chrono::milliseconds period_;
    NowFn now_;
    std::deque<TimePoint> stamps_;
};

/// Owns a callable and a limiter. Refused calls do not invoke the callable.
template <class F>
class RateLimited {
public:
    RateLimited(F fn, SlidingWindowLimiter limiter) : fn_(std::move(fn)), limiter_(std::move(limiter)) {
        not_implemented("RateLimited");
    }

    /// Admitted: the callable's result. Refused: the retry-after duration.
    template <class... Args>
    std::expected<std::invoke_result_t<F&, Args...>, std::chrono::milliseconds> operator()(Args&&... args) {
        not_implemented("RateLimited::operator()");
    }

private:
    F fn_;
    SlidingWindowLimiter limiter_;
};

/// Memoizes R(Args...) with a time-to-live. Keys are std::tuple of the
/// decayed argument types. An entry is a hit while `now < stored + ttl`.
template <class Sig>
class MemoizedTtl;

template <class R, class... Args>
class MemoizedTtl<R(Args...)> {
public:
    using Key = std::tuple<std::decay_t<Args>...>;

    MemoizedTtl(std::function<R(Args...)> fn, std::chrono::milliseconds ttl, NowFn now)
        : fn_(std::move(fn)), ttl_(ttl), now_(std::move(now)) {
        not_implemented("MemoizedTtl");
    }

    /// Returns a cached value when fresh, otherwise calls fn and stores the result.
    R operator()(Args... args) {
        not_implemented("MemoizedTtl::operator()");
    }

    std::size_t hits() const { return hits_; }
    std::size_t misses() const { return misses_; }
    std::size_t size() const { return cache_.size(); }

    /// Drops all entries. Counters are kept.
    void clear() { not_implemented("MemoizedTtl::clear"); }

private:
    struct Entry {
        R value;
        TimePoint stored;
    };

    std::function<R(Args...)> fn_;
    std::chrono::milliseconds ttl_;
    NowFn now_;
    std::map<Key, Entry> cache_;
    std::size_t hits_ = 0;
    std::size_t misses_ = 0;
};

/// Builds a MemoizedTtl for signature Sig, e.g. memoize_ttl<int(int)>(fn, ttl, now).
template <class Sig, class F>
MemoizedTtl<Sig> memoize_ttl(F fn, std::chrono::milliseconds ttl, NowFn now) {
    return MemoizedTtl<Sig>(std::move(fn), ttl, std::move(now));
}

// ===== Part 6: overloads, variants, recursion, constexpr lambdas =====

/// Inherits the call operators of all Ts. The placeholder below hides them
/// and must be replaced by a `using Ts::operator()...;` declaration.
template <class... Ts>
struct overloaded : Ts... {
    std::string operator()(auto&&...) const {
        not_implemented("overloaded::operator()");
    }
};

template <class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

struct Connected {
    std::string peer;
};

struct Message {
    std::string body;
};

struct Disconnected {
    int code = 0;
};

using Event = std::variant<Connected, Message, Disconnected>;

/// "connected <peer>", "message <body>", or "disconnected <code>".
std::string describe(const Event& event);

/// A rose tree.
struct Node {
    std::string name;
    std::vector<Node> children;
};

/// Total number of nodes in the tree rooted at `node`. Implemented as a
/// deducing-this (recursive) lambda.
inline constexpr auto count_nodes = [](this auto const& self, const Node& node) -> std::size_t {
    (void)self;
    (void)node;
    not_implemented("count_nodes");
};

/// One line per node, two spaces of indent per depth, '\n' after every line.
std::string render_tree(const Node& root);

/// True for ports 1..65535. Intended to be usable in constant expressions.
inline constexpr auto is_valid_port = [](int) -> bool { not_implemented("is_valid_port"); };

// ===== Part 7: dispatch and middleware =====

/// A command receives its arguments and returns an exit code.
using CommandFn = std::function<int(std::span<const std::string>)>;

/// Thrown by CommandRegistry::run for an unregistered name.
class UnknownCommand : public std::out_of_range {
public:
    using std::out_of_range::out_of_range;
};

/// Name -> command table. Lookup is heterogeneous (string_view keys work).
class CommandRegistry {
public:
    /// Throws std::invalid_argument for an empty or already-registered name.
    void add(std::string name, CommandFn fn);

    /// Throws UnknownCommand if `name` is not registered.
    int run(std::string_view name, std::span<const std::string> args) const;

    /// Registered names in sorted order.
    std::vector<std::string> names() const;

    std::size_t size() const { return commands_.size(); }

private:
    std::map<std::string, CommandFn, std::less<>> commands_;
};

struct Request {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
};

struct Response {
    int status = 200;
    std::string body;
};

using Handler = std::function<Response(const Request&)>;

/// Wraps a handler. Takes the next handler and returns the new handler.
using Middleware = std::function<Handler(Handler)>;

/// Composes `middleware` around `terminal`. The first element is the outermost.
Handler build_chain(Handler terminal, const std::vector<Middleware>& middleware);

/// Converts exceptions from the inner handler into a 500 response whose body is e.what().
Middleware recover_errors();

/// Responds 400 "missing header: <name>" without calling the inner handler when
/// the request lacks `name`.
Middleware require_header(std::string name);

using LogSink = std::function<void(std::string_view)>;

/// After each response, calls sink with "<METHOD> <path> <status>".
Middleware access_log(LogSink sink);

}  // namespace cppref::lambdas
