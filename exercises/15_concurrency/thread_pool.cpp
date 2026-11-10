#include "thread_pool.hpp"

#include <stdexcept>

namespace cppref::concurrency {

// The stub destructor must not throw; the engineer's implementation calls shutdown(drain) here.
ThreadPool::~ThreadPool() {}

void ThreadPool::shutdown(ShutdownMode mode) {
    (void)mode;
    throw std::logic_error("not implemented: ThreadPool::shutdown");
}

std::size_t ThreadPool::pending() const {
    throw std::logic_error("not implemented: ThreadPool::pending");
}

}  // namespace cppref::concurrency
