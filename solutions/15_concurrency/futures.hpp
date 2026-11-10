#pragma once

#include <exception>
#include <functional>
#include <future>
#include <utility>
#include <vector>

namespace cppref::concurrency {

// Runs every job concurrently (one std::async(std::launch::async) per job) and returns the results
// in input order.
// - Empty input returns an empty vector.
// - Every job is joined before this function returns, even when one throws.
// - If jobs threw, the exception of the lowest-index failing job is rethrown after all jobs finish.
// - R must be non-void.
template <typename R>
std::vector<R> run_all(std::vector<std::function<R()>> jobs) {
    std::vector<std::future<R>> futures;
    futures.reserve(jobs.size());
    for (auto& job : jobs) {
        futures.push_back(std::async(std::launch::async, std::move(job)));
    }

    // Futures are visited in index order, so the first stored exception has the lowest index.
    std::vector<R> results;
    results.reserve(futures.size());
    std::exception_ptr first_error;
    for (auto& f : futures) {
        try {
            results.push_back(f.get());
        } catch (...) {
            if (!first_error) first_error = std::current_exception();
        }
    }
    if (first_error) std::rethrow_exception(first_error);
    return results;
}

}  // namespace cppref::concurrency
