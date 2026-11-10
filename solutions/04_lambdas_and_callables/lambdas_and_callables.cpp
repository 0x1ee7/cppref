// Exercise 04: lambdas and callables (reference solution).
#include "lambdas_and_callables.hpp"

#include <cstddef>
#include <stdexcept>
#include <string>
#include <string_view>

namespace cppref::lambdas {

void not_implemented(std::string_view name) {
    throw std::logic_error("not implemented: " + std::string(name));
}

// ===== Part 1 =====

std::function<int(int)> make_multiplier(int factor) {
    return [factor](int x) { return x * factor; };
}

std::function<int(int)> make_accumulator() {
    return [total = 0](int x) mutable {
        total += x;
        return total;
    };
}

std::vector<std::function<int(int)>> make_adders(int n) {
    if (n < 0) {
        throw std::invalid_argument("make_adders: n must be >= 0");
    }
    std::vector<std::function<int(int)>> adders;
    adders.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        adders.emplace_back([i](int x) { return x + i; });
    }
    return adders;
}

// ===== Part 2 =====

TimePoint SteadyClock::operator()() const {
    return std::chrono::steady_clock::now();
}

// ===== Part 3 =====

void validate_policy(const RetryPolicy& policy) {
    if (policy.max_attempts < 1) {
        throw std::invalid_argument("RetryPolicy: max_attempts must be >= 1");
    }
    if (policy.base_delay.count() < 0) {
        throw std::invalid_argument("RetryPolicy: base_delay must be >= 0");
    }
    if (policy.max_delay < policy.base_delay) {
        throw std::invalid_argument("RetryPolicy: max_delay must be >= base_delay");
    }
    if (policy.multiplier < 1.0) {
        throw std::invalid_argument("RetryPolicy: multiplier must be >= 1.0");
    }
}

// ===== Part 4 =====

int sum_with(std::span<const int> xs, FunctionRef<int(int)> f) {
    int total = 0;
    for (int x : xs) {
        total += f(x);
    }
    return total;
}

std::string tag_line(std::string_view tag, std::string_view body) {
    std::string line;
    line.reserve(tag.size() + body.size() + 3);
    line += '[';
    line += tag;
    line += "] ";
    line += body;
    return line;
}

std::function<std::string(std::string_view)> make_tagger(std::string tag) {
    return [tag = std::move(tag)](std::string_view body) { return tag_line(tag, body); };
}

// ===== Part 5 =====

SlidingWindowLimiter::SlidingWindowLimiter(std::size_t max_calls, std::chrono::milliseconds period, NowFn now)
    : max_calls_(max_calls), period_(period), now_(std::move(now)) {
    if (max_calls_ == 0) {
        throw std::invalid_argument("SlidingWindowLimiter: max_calls must be > 0");
    }
    if (period_.count() <= 0) {
        throw std::invalid_argument("SlidingWindowLimiter: period must be > 0");
    }
}

std::expected<void, std::chrono::milliseconds> SlidingWindowLimiter::try_acquire() {
    const TimePoint now = now_();
    while (!stamps_.empty() && stamps_.front() + period_ <= now) {
        stamps_.pop_front();
    }
    if (stamps_.size() < max_calls_) {
        stamps_.push_back(now);
        return {};
    }
    const TimePoint expires = stamps_.front() + period_;
    return std::unexpected(std::chrono::duration_cast<std::chrono::milliseconds>(expires - now));
}

// ===== Part 6 =====

std::string describe(const Event& event) {
    return std::visit(
        overloaded{
            [](const Connected& c) { return std::string("connected ") + c.peer; },
            [](const Message& m) { return std::string("message ") + m.body; },
            [](const Disconnected& d) { return std::string("disconnected ") + std::to_string(d.code); },
        },
        event);
}

namespace {

// Appends one line per node; deducing-this lambda gives recursion without a named function.
inline constexpr auto render_into = [](this auto const& self, const Node& node, std::size_t depth,
                                       std::string& out) -> void {
    out.append(depth * 2, ' ');
    out += node.name;
    out += '\n';
    for (const auto& child : node.children) {
        self(child, depth + 1, out);
    }
};

}  // namespace

std::string render_tree(const Node& root) {
    std::string out;
    render_into(root, 0, out);
    return out;
}

// ===== Part 7 =====

void CommandRegistry::add(std::string name, CommandFn fn) {
    if (name.empty()) {
        throw std::invalid_argument("CommandRegistry: empty command name");
    }
    if (commands_.contains(name)) {
        throw std::invalid_argument("CommandRegistry: duplicate command: " + name);
    }
    commands_.emplace(std::move(name), std::move(fn));
}

int CommandRegistry::run(std::string_view name, std::span<const std::string> args) const {
    auto it = commands_.find(name);
    if (it == commands_.end()) {
        throw UnknownCommand("unknown command: " + std::string(name));
    }
    return it->second(args);
}

std::vector<std::string> CommandRegistry::names() const {
    std::vector<std::string> out;
    out.reserve(commands_.size());
    for (const auto& entry : commands_) {
        out.push_back(entry.first);
    }
    return out;
}

Handler build_chain(Handler terminal, const std::vector<Middleware>& middleware) {
    Handler result = std::move(terminal);
    for (auto it = middleware.rbegin(); it != middleware.rend(); ++it) {
        result = (*it)(std::move(result));
    }
    return result;
}

Middleware recover_errors() {
    return [](Handler next) -> Handler {
        return [next = std::move(next)](const Request& req) -> Response {
            try {
                return next(req);
            } catch (const std::exception& e) {
                return Response{500, e.what()};
            }
        };
    };
}

Middleware require_header(std::string name) {
    return [name = std::move(name)](Handler next) -> Handler {
        return [name, next = std::move(next)](const Request& req) -> Response {
            if (!req.headers.contains(name)) {
                return Response{400, "missing header: " + name};
            }
            return next(req);
        };
    };
}

Middleware access_log(LogSink sink) {
    return [sink = std::move(sink)](Handler next) -> Handler {
        return [sink, next = std::move(next)](const Request& req) -> Response {
            Response res = next(req);
            sink(req.method + " " + req.path + " " + std::to_string(res.status));
            return res;
        };
    };
}

}  // namespace cppref::lambdas
