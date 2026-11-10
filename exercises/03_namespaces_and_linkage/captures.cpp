#include "captures.hpp"

#include <stdexcept>

namespace cppref::captures {

std::function<int()> make_id_factory(int) {
    throw std::logic_error("not implemented: make_id_factory");
}

std::function<void()> make_incrementer(int&) {
    throw std::logic_error("not implemented: make_incrementer");
}

std::function<int()> make_step_counter(int, int) {
    throw std::logic_error("not implemented: make_step_counter");
}

std::function<bool(std::string_view)> make_allow_list(std::vector<std::string>) {
    throw std::logic_error("not implemented: make_allow_list");
}

Scheduler::Scheduler(int) {
    throw std::logic_error("not implemented: Scheduler::Scheduler");
}

std::function<int(int)> Scheduler::make_adder() const {
    throw std::logic_error("not implemented: Scheduler::make_adder");
}

}  // namespace cppref::captures
