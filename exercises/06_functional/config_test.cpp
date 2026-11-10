#include "config.hpp"
#include "expected_utils.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <expected>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

namespace fn = cppref::functional;

TEST(Part5ConfigParsing, ParsePortAcceptsRangeAndTrims) {
    EXPECT_EQ(fn::parse_port(" 8080 ").value(), 8080);
    EXPECT_EQ(fn::parse_port("1").value(), 1);
    EXPECT_EQ(fn::parse_port("65535").value(), 65535);
}

TEST(Part5ConfigParsing, ParsePortRejectsNonIntegerAndOutOfRange) {
    auto text = fn::parse_port("abc");
    ASSERT_FALSE(text.has_value());
    EXPECT_EQ(text.error(), "port must be an integer, got 'abc'");

    for (std::string_view bad : {"0", "70000", "-1"}) {
        auto r = fn::parse_port(bad);
        ASSERT_FALSE(r.has_value()) << bad;
        EXPECT_EQ(r.error(), "port must be between 1 and 65535") << bad;
    }
}

TEST(Part5ConfigParsing, ParseBoolIsCaseInsensitiveAndStrict) {
    EXPECT_EQ(fn::parse_bool("TRUE").value(), true);
    EXPECT_EQ(fn::parse_bool("Off").value(), false);
    EXPECT_EQ(fn::parse_bool("yes").value(), true);
    EXPECT_EQ(fn::parse_bool("0").value(), false);

    auto bad = fn::parse_bool("maybe");
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error(), "expected boolean, got 'maybe'");
}

TEST(Part5ConfigParsing, ParsePortsReturnsAllOrFirstError) {
    EXPECT_EQ(fn::parse_ports("80, 443 ,8080").value(), (std::vector<std::uint16_t>{80, 443, 8080}));

    auto bad = fn::parse_ports("80,x,0");
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error(), "port must be an integer, got 'x'");
}

TEST(Part5ConfigParsing, CollectStopsAtFirstErrorWithoutEvaluatingTheRest) {
    int calls = 0;
    auto results = std::views::iota(1, 10) | std::views::transform([&calls](int i) {
                       ++calls;
                       if (i == 3) {
                           return std::expected<int, std::string>{std::unexpect, "bad 3"};
                       }
                       return std::expected<int, std::string>{i * 10};
                   });
    auto out = fn::collect(results);
    ASSERT_FALSE(out.has_value());
    EXPECT_EQ(out.error(), "bad 3");
    EXPECT_EQ(calls, 3);
}

TEST(Part5ConfigParsing, TryCallCapturesExceptionsAsValues) {
    auto ok = fn::try_call([](int x) { return x * 2; }, 21);
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(*ok, 42);

    auto failing = fn::try_call([](int) -> int { throw std::runtime_error("boom"); }, 1);
    ASSERT_FALSE(failing.has_value());
    EXPECT_THROW(std::rethrow_exception(failing.error()), std::runtime_error);
}

TEST(Part5ConfigParsing, ParseConfigMinimal) {
    auto r = fn::parse_config("host = example.com\nport = 8080");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, (fn::Config{"example.com", 8080, false, 4}));
}

TEST(Part5ConfigParsing, ParseConfigSkipsCommentsAndBlankLines) {
    auto r = fn::parse_config("# service\nhost = api.example.com\n\nport=8443\ndebug = yes\nworkers = 16\n");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, (fn::Config{"api.example.com", 8443, true, 16}));
}

TEST(Part5ConfigParsing, ParseConfigCollectsEveryErrorInLineOrder) {
    auto r = fn::parse_config("host = a\nport = 99999\ncolour = red\nworkers = 0\noops\nhost = b\n");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), (std::vector<std::string>{
                             "line 2: port must be between 1 and 65535",
                             "line 3: unknown key 'colour'",
                             "line 4: workers must be between 1 and 64",
                             "line 5: expected 'key = value'",
                             "line 6: duplicate key 'host'",
                         }));
}

TEST(Part5ConfigParsing, ParseConfigReportsMissingRequiredKeys) {
    auto r = fn::parse_config("debug = no");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), (std::vector<std::string>{"missing required key 'host'",
                                                   "missing required key 'port'"}));
}

TEST(Part5ConfigParsing, ParseConfigRejectsEmptyHostAndBadBool) {
    auto r = fn::parse_config("host =\nport = 1\ndebug = maybe");
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), (std::vector<std::string>{"line 1: host must not be empty",
                                                   "line 3: expected boolean, got 'maybe'"}));
}

TEST(Part5ConfigParsing, ParseConfigNeverThrowsOnGarbage) {
    const std::string junk = "=\n==\n\x01\nport\nworkers = abc\nhost = x\nport = 1\n";
    EXPECT_NO_THROW({
        auto r = fn::parse_config(junk);
        EXPECT_FALSE(r.has_value());
    });
}
