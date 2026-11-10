#include "batch.hpp"

#include <array>
#include <exception>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

namespace cppref::error_handling {

BatchError::BatchError(std::size_t failed, std::size_t total, std::vector<TaskFailure> failures)
    : DomainError(std::to_string(failed) + " of " + std::to_string(total) + " tasks failed"),
      failures_(std::move(failures)) {}

const std::vector<TaskFailure>& BatchError::failures() const {
    return failures_;
}

std::string failure_message(const std::exception_ptr& error) {
    if (!error) {
        return "no error";
    }
    try {
        std::rethrow_exception(error);
    } catch (const std::exception& e) {
        return e.what();
    } catch (...) {
        return "unknown exception";
    }
}

namespace {

constexpr std::array kTransientCodes{
    std::errc::timed_out,
    std::errc::connection_refused,
    std::errc::connection_reset,
    std::errc::connection_aborted,
    std::errc::interrupted,
    std::errc::resource_unavailable_try_again,
    std::errc::device_or_resource_busy,
};

}  // namespace

bool is_transient(const std::exception& error) {
    const auto* sys = dynamic_cast<const std::system_error*>(&error);
    if (sys == nullptr) {
        return false;
    }
    for (const std::errc code : kTransientCodes) {
        if (sys->code() == code) {
            return true;
        }
    }
    return false;
}

bool is_transient(const std::exception_ptr& error) {
    if (!error) {
        return false;
    }
    try {
        std::rethrow_exception(error);
    } catch (const std::exception& e) {
        return is_transient(e);
    } catch (...) {
        return false;
    }
}

RetryExhausted::RetryExhausted(int attempts)
    : DomainError("gave up after " + std::to_string(attempts) + " attempts"), attempts_(attempts) {}

int RetryExhausted::attempts() const {
    return attempts_;
}

TriagedFailures triage(const BatchError& error) {
    TriagedFailures out;
    for (const TaskFailure& failure : error.failures()) {
        if (is_transient(failure.error)) {
            out.transient.push_back(failure);
        } else {
            out.permanent.push_back(failure);
        }
    }
    return out;
}

}  // namespace cppref::error_handling
