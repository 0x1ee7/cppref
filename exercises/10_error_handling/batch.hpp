#pragma once

#include "errors.hpp"

#include <chrono>
#include <cstddef>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace cppref::error_handling {

/// A failed task. The exception stays an std::exception_ptr so its dynamic type
/// survives until the caller rethrows it.
struct TaskFailure {
    std::string name;
    std::exception_ptr error;
};

/// Aggregate error, the analogue of ExceptionGroup. run_batch throws it when at
/// least one task failed. what() reads "N of M tasks failed"; failures() are in
/// task order.
class BatchError : public DomainError {
public:
    BatchError(std::size_t failed, std::size_t total, std::vector<TaskFailure> failures);

    [[nodiscard]] const std::vector<TaskFailure>& failures() const;

private:
    std::vector<TaskFailure> failures_;
};

/// Text of a captured exception: what() for std::exception, otherwise
/// "unknown exception". A null exception_ptr yields "no error".
[[nodiscard]] std::string failure_message(const std::exception_ptr& error);

/// A named unit of work producing T.
template <typename T>
struct NamedTask {
    std::string name;
    std::function<T()> run;
};

/// Runs every task in order, even after failures. Any exception thrown by a
/// task is captured (catch-all), recorded with its name, and the batch goes on.
/// Duplicate names throw std::invalid_argument before any task runs.
/// Returns (name, result) pairs in task order when everything succeeds;
/// otherwise throws BatchError.
template <typename T>
[[nodiscard]] std::vector<std::pair<std::string, T>> run_batch(
    [[maybe_unused]] const std::vector<NamedTask<T>>& tasks) {
    throw std::logic_error("not implemented: run_batch");
}

/// Retry classification. True for std::system_error whose code is one of:
/// timed_out, connection_refused, connection_reset, connection_aborted,
/// interrupted, resource_unavailable_try_again, device_or_resource_busy.
/// Everything else, including non-std exceptions, is permanent.
[[nodiscard]] bool is_transient(const std::exception& error);

/// Same classification for a captured exception. A null pointer is not transient.
[[nodiscard]] bool is_transient(const std::exception_ptr& error);

/// Thrown by retry_call after the final transient failure. The last error is
/// nested under it. Message: "gave up after N attempts".
class RetryExhausted : public DomainError {
public:
    explicit RetryExhausted(int attempts);

    [[nodiscard]] int attempts() const;

private:
    int attempts_;
};

/// Retry settings. `sleep` and `classify` are injectable so tests need no real time.
struct RetryPolicy {
    int attempts = 3;
    std::chrono::milliseconds base_delay{100};
    std::function<void(std::chrono::milliseconds)> sleep = [](std::chrono::milliseconds d) {
        std::this_thread::sleep_for(d);
    };
    std::function<bool(const std::exception&)> classify = [](const std::exception& e) {
        return is_transient(e);
    };
};

/// Calls `fn` up to policy.attempts times and returns its result.
///   - attempts < 1 -> std::invalid_argument, and fn is never called.
///   - a permanent error (classify is false, or a non-std exception) is
///     rethrown at once, with no sleep.
///   - a transient error sleeps base_delay * 2^(n-1) before attempt n+1, where
///     n is the attempt that just failed. There is no sleep after the last attempt.
///   - after the final transient failure, throws RetryExhausted(attempts)
///     with the last error nested under it.
template <typename F>
auto retry_call([[maybe_unused]] F&& fn, [[maybe_unused]] const RetryPolicy& policy)
    -> std::invoke_result_t<F&> {
    throw std::logic_error("not implemented: retry_call");
}

/// Failures of one batch split by retry classification.
struct TriagedFailures {
    std::vector<TaskFailure> transient;
    std::vector<TaskFailure> permanent;
};

/// Splits a BatchError's failures using is_transient, keeping task order.
[[nodiscard]] TriagedFailures triage(const BatchError& error);

}  // namespace cppref::error_handling
