// Exercise 04: lambdas and callables (reference solution).
#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <deque>
#include <expected>
#include <functional>
#include <map>
#include <memory>
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

[[noreturn]] void not_implemented(std::string_view name);

using TimePoint = std::chrono::steady_clock::time_point;
using NowFn = std::function<TimePoint()>;

// ===== Part 1: closures and composition =====

std::function<int(int)> make_multiplier(int factor);
std::function<int(int)> make_accumulator();
std::vector<std::function<int(int)>> make_adders(int n);

namespace detail {

struct Identity {
    template <class T>
    constexpr T operator()(T&& x) const {
        return std::forward<T>(x);
    }
};

// Calls f(g(args...)). Both stored by value; both must be const-invocable.
template <class F, class G>
struct Compose2 {
    F f;
    G g;

    template <class... Args>
    constexpr decltype(auto) operator()(Args&&... args) const {
        return std::invoke(f, std::invoke(g, std::forward<Args>(args)...));
    }
};

constexpr Identity compose_impl() noexcept {
    return {};
}

template <class F, class... Rest>
constexpr auto compose_impl(F f, Rest... rest) {
    if constexpr (sizeof...(Rest) == 0) {
        return f;
    } else {
        using Tail = decltype(compose_impl(std::declval<Rest>()...));
        return Compose2<F, Tail>{std::move(f), compose_impl(std::move(rest)...)};
    }
}

}  // namespace detail

template <class... Fs>
constexpr auto compose(Fs... fs) {
    return detail::compose_impl(std::move(fs)...);
}

// ===== Part 2: forwarding decorator =====

struct Timing {
    std::string label;
    std::chrono::nanoseconds elapsed{};
    bool threw = false;
};

using TimingSink = std::function<void(const Timing&)>;

struct SteadyClock {
    TimePoint operator()() const;
};

template <class Fn, class Clock = SteadyClock>
class Timed {
public:
    Timed(std::string label, Fn fn, TimingSink sink, Clock clock)
        : label_(std::move(label)), fn_(std::move(fn)), sink_(std::move(sink)), clock_(std::move(clock)) {}

    template <class... Args>
    std::invoke_result_t<Fn&, Args...> operator()(Args&&... args) {
        using R = std::invoke_result_t<Fn&, Args...>;
        const TimePoint start = clock_();
        auto report = [&](bool threw) {
            const TimePoint end = clock_();
            sink_(Timing{label_, std::chrono::duration_cast<std::chrono::nanoseconds>(end - start), threw});
        };
        if constexpr (std::is_void_v<R>) {
            try {
                std::invoke(fn_, std::forward<Args>(args)...);
            } catch (...) {
                report(true);
                throw;
            }
            report(false);
        } else {
            R result = [&]() -> R {
                try {
                    return std::invoke(fn_, std::forward<Args>(args)...);
                } catch (...) {
                    report(true);
                    throw;
                }
            }();
            report(false);
            return result;
        }
    }

private:
    std::string label_;
    Fn fn_;
    TimingSink sink_;
    Clock clock_;
};

template <class Fn, class Clock = SteadyClock>
Timed<Fn, Clock> timed(std::string label, Fn fn, TimingSink sink, Clock clock = Clock{}) {
    return Timed<Fn, Clock>(std::move(label), std::move(fn), std::move(sink), std::move(clock));
}

// ===== Part 3: retry with backoff =====

struct RetryPolicy {
    int max_attempts = 3;
    std::chrono::milliseconds base_delay{100};
    double multiplier = 2.0;
    std::chrono::milliseconds max_delay{5000};
    std::function<bool(const std::error_code&)> retryable = [](const std::error_code&) { return true; };
};

using SleepFn = std::function<void(std::chrono::milliseconds)>;

void validate_policy(const RetryPolicy& policy);

template <class Fn>
class Retrying {
public:
    Retrying(Fn fn, RetryPolicy policy, SleepFn sleep)
        : fn_(std::move(fn)), policy_(std::move(policy)), sleep_(std::move(sleep)) {}

    template <class... Args>
    std::invoke_result_t<Fn&, Args&...> operator()(Args&&... args) {
        using R = std::invoke_result_t<Fn&, Args&...>;
        for (int attempt = 1;; ++attempt) {
            R result = std::invoke(fn_, args...);
            if (result || attempt >= policy_.max_attempts || !policy_.retryable(result.error())) {
                return result;
            }
            const double wanted =
                static_cast<double>(policy_.base_delay.count()) * std::pow(policy_.multiplier, attempt - 1);
            const double capped = std::min(wanted, static_cast<double>(policy_.max_delay.count()));
            sleep_(std::chrono::milliseconds{static_cast<std::chrono::milliseconds::rep>(capped)});
        }
    }

private:
    Fn fn_;
    RetryPolicy policy_;
    SleepFn sleep_;
};

template <class Fn>
Retrying<Fn> retry(Fn fn, RetryPolicy policy, SleepFn sleep) {
    validate_policy(policy);
    return Retrying<Fn>(std::move(fn), std::move(policy), std::move(sleep));
}

// ===== Part 4: type erasure without ownership =====

template <class Sig>
class FunctionRef;

template <class R, class... Args>
class FunctionRef<R(Args...)> {
public:
    template <class F>
        requires(!std::same_as<std::remove_cvref_t<F>, FunctionRef>) && std::is_invocable_r_v<R, F&, Args...>
    FunctionRef(F& f) noexcept
        : obj_(const_cast<void*>(static_cast<const void*>(std::addressof(f)))),
          call_([](void* o, Args... a) -> R {
              return std::invoke(*static_cast<F*>(o), std::forward<Args>(a)...);
          }) {}

    R operator()(Args... args) const {
        return call_(obj_, std::forward<Args>(args)...);
    }

private:
    void* obj_ = nullptr;
    R (*call_)(void*, Args...) = nullptr;
};

int sum_with(std::span<const int> xs, FunctionRef<int(int)> f);
std::string tag_line(std::string_view tag, std::string_view body);
std::function<std::string(std::string_view)> make_tagger(std::string tag);

// ===== Part 5: stateful callables =====

template <class F>
class CallCounter {
public:
    explicit CallCounter(F fn) : fn_(std::move(fn)) {}

    template <class... Args>
    std::invoke_result_t<F&, Args...> operator()(Args&&... args) {
        ++count_;
        return std::invoke(fn_, std::forward<Args>(args)...);
    }

    std::size_t count() const { return count_; }
    void reset() { count_ = 0; }

private:
    F fn_;
    std::size_t count_ = 0;
};

class SlidingWindowLimiter {
public:
    SlidingWindowLimiter(std::size_t max_calls, std::chrono::milliseconds period, NowFn now);

    std::expected<void, std::chrono::milliseconds> try_acquire();

private:
    std::size_t max_calls_;
    std::chrono::milliseconds period_;
    NowFn now_;
    std::deque<TimePoint> stamps_;
};

template <class F>
class RateLimited {
public:
    RateLimited(F fn, SlidingWindowLimiter limiter) : fn_(std::move(fn)), limiter_(std::move(limiter)) {}

    template <class... Args>
    std::expected<std::invoke_result_t<F&, Args...>, std::chrono::milliseconds> operator()(Args&&... args) {
        using R = std::invoke_result_t<F&, Args...>;
        using Out = std::expected<R, std::chrono::milliseconds>;
        if (auto admitted = limiter_.try_acquire(); !admitted) {
            return std::unexpected(admitted.error());
        }
        if constexpr (std::is_void_v<R>) {
            std::invoke(fn_, std::forward<Args>(args)...);
            return Out{};
        } else {
            return Out(std::in_place, std::invoke(fn_, std::forward<Args>(args)...));
        }
    }

private:
    F fn_;
    SlidingWindowLimiter limiter_;
};

template <class Sig>
class MemoizedTtl;

template <class R, class... Args>
class MemoizedTtl<R(Args...)> {
public:
    using Key = std::tuple<std::decay_t<Args>...>;

    MemoizedTtl(std::function<R(Args...)> fn, std::chrono::milliseconds ttl, NowFn now)
        : fn_(std::move(fn)), ttl_(ttl), now_(std::move(now)) {}

    R operator()(Args... args) {
        Key key{args...};
        const TimePoint now = now_();
        if (auto it = cache_.find(key); it != cache_.end() && now < it->second.stored + ttl_) {
            ++hits_;
            return it->second.value;
        }
        ++misses_;
        R value = fn_(args...);
        cache_.insert_or_assign(std::move(key), Entry{value, now});
        return value;
    }

    std::size_t hits() const { return hits_; }
    std::size_t misses() const { return misses_; }
    std::size_t size() const { return cache_.size(); }
    void clear() { cache_.clear(); }

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

template <class Sig, class F>
MemoizedTtl<Sig> memoize_ttl(F fn, std::chrono::milliseconds ttl, NowFn now) {
    return MemoizedTtl<Sig>(std::move(fn), ttl, std::move(now));
}

// ===== Part 6: overloads, variants, recursion, constexpr lambdas =====

template <class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
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

std::string describe(const Event& event);

struct Node {
    std::string name;
    std::vector<Node> children;
};

inline constexpr auto count_nodes = [](this auto const& self, const Node& node) -> std::size_t {
    std::size_t total = 1;
    for (const auto& child : node.children) {
        total += self(child);
    }
    return total;
};

std::string render_tree(const Node& root);

inline constexpr auto is_valid_port = [](int port) constexpr noexcept -> bool {
    return port >= 1 && port <= 65535;
};

// ===== Part 7: dispatch and middleware =====

using CommandFn = std::function<int(std::span<const std::string>)>;

class UnknownCommand : public std::out_of_range {
public:
    using std::out_of_range::out_of_range;
};

class CommandRegistry {
public:
    void add(std::string name, CommandFn fn);
    int run(std::string_view name, std::span<const std::string> args) const;
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
using Middleware = std::function<Handler(Handler)>;

Handler build_chain(Handler terminal, const std::vector<Middleware>& middleware);
Middleware recover_errors();
Middleware require_header(std::string name);

using LogSink = std::function<void(std::string_view)>;
Middleware access_log(LogSink sink);

}  // namespace cppref::lambdas
