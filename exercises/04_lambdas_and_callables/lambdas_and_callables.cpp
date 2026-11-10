// Exercise 04 stub: non-template definitions. Replace each body.
#include "lambdas_and_callables.hpp"

#include <stdexcept>
#include <string>

namespace cppref::lambdas {

void not_implemented(std::string_view name) {
    throw std::logic_error("not implemented: " + std::string(name));
}

// Part 1
std::function<int(int)> make_multiplier(int) { not_implemented("make_multiplier"); }
std::function<int(int)> make_accumulator() { not_implemented("make_accumulator"); }
std::vector<std::function<int(int)>> make_adders(int) { not_implemented("make_adders"); }

// Part 2
TimePoint SteadyClock::operator()() const { not_implemented("SteadyClock"); }

// Part 3
void validate_policy(const RetryPolicy&) { not_implemented("validate_policy"); }

// Part 4
int sum_with(std::span<const int>, FunctionRef<int(int)>) { not_implemented("sum_with"); }
std::string tag_line(std::string_view, std::string_view) { not_implemented("tag_line"); }
std::function<std::string(std::string_view)> make_tagger(std::string) { not_implemented("make_tagger"); }

// Part 5
SlidingWindowLimiter::SlidingWindowLimiter(std::size_t, std::chrono::milliseconds, NowFn)
    : max_calls_(0), period_(0) {
    not_implemented("SlidingWindowLimiter");
}

std::expected<void, std::chrono::milliseconds> SlidingWindowLimiter::try_acquire() {
    not_implemented("SlidingWindowLimiter::try_acquire");
}

// Part 6
std::string describe(const Event&) { not_implemented("describe"); }
std::string render_tree(const Node&) { not_implemented("render_tree"); }

// Part 7
void CommandRegistry::add(std::string, CommandFn) { not_implemented("CommandRegistry::add"); }
int CommandRegistry::run(std::string_view, std::span<const std::string>) const {
    not_implemented("CommandRegistry::run");
}
std::vector<std::string> CommandRegistry::names() const { not_implemented("CommandRegistry::names"); }

Handler build_chain(Handler, const std::vector<Middleware>&) { not_implemented("build_chain"); }
Middleware recover_errors() { not_implemented("recover_errors"); }
Middleware require_header(std::string) { not_implemented("require_header"); }
Middleware access_log(LogSink) { not_implemented("access_log"); }

}  // namespace cppref::lambdas
