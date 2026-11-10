#pragma once

#include <chrono>
#include <functional>

namespace cppref::stl {

/// Time source injected into time-dependent containers. Production code uses the
/// default; tests pass a fake that they advance by hand.
using MonotonicClock = std::function<std::chrono::steady_clock::time_point()>;

/// Default source: `std::chrono::steady_clock::now()`.
inline MonotonicClock steady_clock_source() {
    return [] { return std::chrono::steady_clock::now(); };
}

}  // namespace cppref::stl
