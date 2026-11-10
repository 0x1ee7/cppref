#pragma once

#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace cppref::concurrency {

// Runs every job concurrently (one std::async(std::launch::async) per job) and returns the results
// in input order.
// - Empty input returns an empty vector.
// - Every job is joined before this function returns, even when one throws.
// - If jobs threw, the exception of the lowest-index failing job is rethrown after all jobs finish.
template <typename R>
std::vector<R> run_all(std::vector<std::function<R()>> jobs) {
    (void)jobs;
    throw std::logic_error("not implemented: run_all");
}

}  // namespace cppref::concurrency
