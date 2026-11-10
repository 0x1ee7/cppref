#include "batch.hpp"

#include <stdexcept>

namespace cppref::error_handling {

BatchError::BatchError(std::size_t /*failed*/, std::size_t /*total*/, std::vector<TaskFailure> /*failures*/)
    : DomainError("") {
    throw std::logic_error("not implemented: BatchError::BatchError");
}

const std::vector<TaskFailure>& BatchError::failures() const {
    throw std::logic_error("not implemented: BatchError::failures");
}

std::string failure_message(const std::exception_ptr& /*error*/) {
    throw std::logic_error("not implemented: failure_message");
}

bool is_transient(const std::exception& /*error*/) {
    throw std::logic_error("not implemented: is_transient");
}

bool is_transient(const std::exception_ptr& /*error*/) {
    throw std::logic_error("not implemented: is_transient(exception_ptr)");
}

RetryExhausted::RetryExhausted(int /*attempts*/) : DomainError("") {
    throw std::logic_error("not implemented: RetryExhausted::RetryExhausted");
}

int RetryExhausted::attempts() const {
    throw std::logic_error("not implemented: RetryExhausted::attempts");
}

TriagedFailures triage(const BatchError& /*error*/) {
    throw std::logic_error("not implemented: triage");
}

}  // namespace cppref::error_handling
