#include "scheduler.hpp"

#include <stdexcept>

namespace cppref::concurrency {

// The stub destructor must not throw; the engineer's implementation calls stop() here.
PeriodicScheduler::~PeriodicScheduler() {}

void PeriodicScheduler::add(std::string name, std::chrono::milliseconds interval,
                            std::function<void()> fn) {
    (void)name;
    (void)interval;
    (void)fn;
    throw std::logic_error("not implemented: PeriodicScheduler::add");
}

void PeriodicScheduler::start() {
    throw std::logic_error("not implemented: PeriodicScheduler::start");
}

void PeriodicScheduler::stop() {
    throw std::logic_error("not implemented: PeriodicScheduler::stop");
}

std::vector<JobError> PeriodicScheduler::errors() const {
    throw std::logic_error("not implemented: PeriodicScheduler::errors");
}

}  // namespace cppref::concurrency
