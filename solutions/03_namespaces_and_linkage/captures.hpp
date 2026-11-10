#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

// Part 5 — lambda captures and lifetimes: by value, by reference, mutable state.
namespace cppref::captures {

// Returns a factory yielding start, start+1, start+2, ... Each factory owns its own state.
std::function<int()> make_id_factory(int start);

// Returns a function that adds 1 to `target` on each call. Captures by reference:
// `target` must outlive the returned function.
std::function<void()> make_incrementer(int& target);

// Returns a function yielding start, start+step, start+2*step, ... The mutable state lives
// in the closure, so copies of the returned std::function advance independently.
std::function<int()> make_step_counter(int start, int step);

// Returns a predicate that is true iff its argument is in `allowed`. The vector is taken by
// value and owned by the closure, so the caller may mutate or destroy its own copy.
std::function<bool(std::string_view)> make_allow_list(std::vector<std::string> allowed);

class Scheduler {
public:
    explicit Scheduler(int base);

    // Returns a function that adds this scheduler's base to its argument. The returned
    // function must stay valid after the Scheduler itself is destroyed.
    std::function<int(int)> make_adder() const;

private:
    int base_;
};

}  // namespace cppref::captures
