#include "shutdown.hpp"

#include <stdexcept>

namespace cppref::deployment {

SignalGuard::SignalGuard(std::initializer_list<int>) {
    throw std::logic_error("not implemented: SignalGuard");
}

SignalGuard::~SignalGuard() = default;

std::stop_token SignalGuard::token() const {
    throw std::logic_error("not implemented: SignalGuard::token");
}

std::optional<int> SignalGuard::received_signal() const {
    throw std::logic_error("not implemented: SignalGuard::received_signal");
}

}  // namespace cppref::deployment
