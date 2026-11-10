#include "rate_limiter.hpp"

#include <stdexcept>

namespace cppref::stl {

SlidingWindowLimiter::SlidingWindowLimiter(std::size_t, std::chrono::milliseconds, MonotonicClock) {
    throw std::logic_error("not implemented: SlidingWindowLimiter::SlidingWindowLimiter");
}

bool SlidingWindowLimiter::allow(std::string_view) {
    throw std::logic_error("not implemented: SlidingWindowLimiter::allow");
}

std::size_t SlidingWindowLimiter::remaining(std::string_view) const {
    throw std::logic_error("not implemented: SlidingWindowLimiter::remaining");
}

std::chrono::milliseconds SlidingWindowLimiter::retry_after(std::string_view) const {
    throw std::logic_error("not implemented: SlidingWindowLimiter::retry_after");
}

std::size_t SlidingWindowLimiter::prune() {
    throw std::logic_error("not implemented: SlidingWindowLimiter::prune");
}

}  // namespace cppref::stl
