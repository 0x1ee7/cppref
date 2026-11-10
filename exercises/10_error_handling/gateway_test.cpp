#include "gateway.hpp"

#include <gtest/gtest.h>

#include <set>
#include <string>
#include <system_error>

namespace eh = cppref::error_handling;
using eh::GatewayErrc;

// --- Part 5: custom std::error_code and std::error_condition -----------------

TEST(Part5ErrorCode, CategoryIsNamedPaymentGateway) {
    EXPECT_EQ(std::string(eh::gateway_category().name()), "payment-gateway");
}

TEST(Part5ErrorCode, MakeErrorCodeCarriesCategoryAndValue) {
    const std::error_code ec = eh::make_error_code(GatewayErrc::card_declined);
    EXPECT_EQ(ec.category(), eh::gateway_category());
    EXPECT_EQ(ec.value(), static_cast<int>(GatewayErrc::card_declined));
    EXPECT_FALSE(static_cast<bool>(eh::make_error_code(GatewayErrc::ok)));  // ok is the zero (success) code
}

TEST(Part5ErrorCode, EveryEnumeratorHasADistinctNonEmptyMessage) {
    std::set<std::string> messages;
    for (int v = 0; v <= static_cast<int>(GatewayErrc::network_down); ++v) {
        const std::string text = eh::gateway_category().message(v);
        EXPECT_FALSE(text.empty()) << "value " << v;
        messages.insert(text);
    }
    EXPECT_EQ(messages.size(), 7u);
}

TEST(Part5ErrorCode, DefaultConditionsMapToStandardConditions) {
    EXPECT_TRUE(eh::make_error_code(GatewayErrc::gateway_timeout) == std::errc::timed_out);
    EXPECT_TRUE(eh::make_error_code(GatewayErrc::rate_limited) == std::errc::resource_unavailable_try_again);
    EXPECT_TRUE(eh::make_error_code(GatewayErrc::network_down) == std::errc::network_unreachable);
}

TEST(Part5ErrorCode, UnmappedCodesUseTheCategoryOwnCondition) {
    const std::error_condition cond = eh::make_error_code(GatewayErrc::card_declined).default_error_condition();
    EXPECT_EQ(cond.category(), eh::gateway_category());
    EXPECT_EQ(cond.value(), static_cast<int>(GatewayErrc::card_declined));
}

TEST(Part5ErrorCode, FromHttpStatusMapsGatewayResponses) {
    EXPECT_EQ(eh::from_http_status(200), GatewayErrc::ok);
    EXPECT_EQ(eh::from_http_status(402), GatewayErrc::card_declined);
    EXPECT_EQ(eh::from_http_status(409), GatewayErrc::insufficient_funds);
    EXPECT_EQ(eh::from_http_status(408), GatewayErrc::gateway_timeout);
    EXPECT_EQ(eh::from_http_status(504), GatewayErrc::gateway_timeout);
    EXPECT_EQ(eh::from_http_status(429), GatewayErrc::rate_limited);
    EXPECT_EQ(eh::from_http_status(503), GatewayErrc::network_down);
    EXPECT_EQ(eh::from_http_status(400), GatewayErrc::invalid_request);
    EXPECT_EQ(eh::from_http_status(404), GatewayErrc::invalid_request);
}

TEST(Part5ErrorCode, ThrowGatewayErrorCarriesCodeAndDetail) {
    bool caught = false;
    try {
        eh::throw_gateway_error(GatewayErrc::rate_limited, "retry later");
    } catch (const std::system_error& e) {
        caught = true;
        EXPECT_EQ(e.code(), eh::make_error_code(GatewayErrc::rate_limited));
        EXPECT_NE(std::string(e.what()).find("retry later"), std::string::npos);
    }
    EXPECT_TRUE(caught) << "expected std::system_error";
}
