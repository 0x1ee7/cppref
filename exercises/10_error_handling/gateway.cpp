#include "gateway.hpp"

#include <stdexcept>

namespace cppref::error_handling {

const std::error_category& gateway_category() {
    throw std::logic_error("not implemented: gateway_category");
}

std::error_code make_error_code(GatewayErrc /*code*/) {
    throw std::logic_error("not implemented: make_error_code");
}

GatewayErrc from_http_status(int /*status*/) {
    throw std::logic_error("not implemented: from_http_status");
}

void throw_gateway_error(GatewayErrc /*code*/, const std::string& /*detail*/) {
    throw std::logic_error("not implemented: throw_gateway_error");
}

}  // namespace cppref::error_handling
