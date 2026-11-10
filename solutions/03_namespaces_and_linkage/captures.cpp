#include "captures.hpp"

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppref::captures {

std::function<int()> make_id_factory(int start) {
    return [next = start]() mutable { return next++; };
}

std::function<void()> make_incrementer(int& target) {
    return [&target] { ++target; };
}

std::function<int()> make_step_counter(int start, int step) {
    return [next = start, step]() mutable {
        const int value = next;
        next += step;
        return value;
    };
}

std::function<bool(std::string_view)> make_allow_list(std::vector<std::string> allowed) {
    return [allowed = std::move(allowed)](std::string_view name) {
        for (const auto& entry : allowed) {
            if (std::string_view(entry) == name) return true;
        }
        return false;
    };
}

Scheduler::Scheduler(int base) : base_(base) {}

std::function<int(int)> Scheduler::make_adder() const {
    // Capture the base by value: no `this`, so the adder outlives the Scheduler.
    return [base = base_](int x) { return x + base; };
}

}  // namespace cppref::captures
