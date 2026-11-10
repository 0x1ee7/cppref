#pragma once

#include <string>
#include <system_error>

namespace cppref::error_handling {

/// Outcomes reported by the (fictional) payment gateway. `ok` is the only
/// success value and maps to a zero error_code.
enum class GatewayErrc {
    ok = 0,
    card_declined,
    insufficient_funds,
    invalid_request,
    gateway_timeout,
    rate_limited,
    network_down,
};

/// Singleton category named "payment-gateway". Engineer implements:
///   name()                      -> "payment-gateway"
///   message(code)               -> a distinct, non-empty text per enumerator
///   default_error_condition()   -> gateway_timeout:  errc::timed_out
///                                  rate_limited:    errc::resource_unavailable_try_again
///                                  network_down:    errc::network_unreachable
///                                  others:          the category's own condition
[[nodiscard]] const std::error_category& gateway_category();

[[nodiscard]] std::error_code make_error_code(GatewayErrc code);

/// Maps an HTTP status from the gateway:
///   2xx -> ok, 402 -> card_declined, 409 -> insufficient_funds,
///   408 and 504 -> gateway_timeout, 429 -> rate_limited,
///   other 5xx -> network_down, anything else -> invalid_request.
[[nodiscard]] GatewayErrc from_http_status(int status);

/// Throws std::system_error(make_error_code(code), detail).
[[noreturn]] void throw_gateway_error(GatewayErrc code, const std::string& detail);

}  // namespace cppref::error_handling

namespace std {
template <>
struct is_error_code_enum<cppref::error_handling::GatewayErrc> : true_type {};
}  // namespace std
