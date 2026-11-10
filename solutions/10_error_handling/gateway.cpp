#include "gateway.hpp"

#include <string>
#include <system_error>

namespace cppref::error_handling {

namespace {

class GatewayCategory final : public std::error_category {
public:
    [[nodiscard]] const char* name() const noexcept override { return "payment-gateway"; }

    [[nodiscard]] std::string message(int code) const override {
        switch (static_cast<GatewayErrc>(code)) {
            case GatewayErrc::ok:
                return "ok";
            case GatewayErrc::card_declined:
                return "card declined";
            case GatewayErrc::insufficient_funds:
                return "insufficient funds";
            case GatewayErrc::invalid_request:
                return "invalid request";
            case GatewayErrc::gateway_timeout:
                return "gateway timed out";
            case GatewayErrc::rate_limited:
                return "rate limited";
            case GatewayErrc::network_down:
                return "network down";
        }
        return "unknown gateway error";
    }

    [[nodiscard]] std::error_condition default_error_condition(int code) const noexcept override {
        switch (static_cast<GatewayErrc>(code)) {
            case GatewayErrc::gateway_timeout:
                return std::make_error_condition(std::errc::timed_out);
            case GatewayErrc::rate_limited:
                return std::make_error_condition(std::errc::resource_unavailable_try_again);
            case GatewayErrc::network_down:
                return std::make_error_condition(std::errc::network_unreachable);
            default:
                return std::error_condition(code, *this);
        }
    }
};

}  // namespace

const std::error_category& gateway_category() {
    static const GatewayCategory instance;
    return instance;
}

std::error_code make_error_code(GatewayErrc code) {
    return {static_cast<int>(code), gateway_category()};
}

GatewayErrc from_http_status(int status) {
    if (status >= 200 && status < 300) {
        return GatewayErrc::ok;
    }
    if (status == 402) {
        return GatewayErrc::card_declined;
    }
    if (status == 409) {
        return GatewayErrc::insufficient_funds;
    }
    if (status == 408 || status == 504) {
        return GatewayErrc::gateway_timeout;
    }
    if (status == 429) {
        return GatewayErrc::rate_limited;
    }
    if (status >= 500 && status < 600) {
        return GatewayErrc::network_down;
    }
    return GatewayErrc::invalid_request;
}

void throw_gateway_error(GatewayErrc code, const std::string& detail) {
    throw std::system_error(make_error_code(code), detail);
}

}  // namespace cppref::error_handling
