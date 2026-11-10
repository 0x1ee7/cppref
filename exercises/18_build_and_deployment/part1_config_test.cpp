#include "config.hpp"

#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace dep = cppref::deployment;
using namespace std::chrono_literals;

namespace {

dep::EnvLookup env_from(std::map<std::string, std::string> vars) {
    return [vars = std::move(vars)](std::string_view name) -> std::optional<std::string> {
        auto it = vars.find(std::string(name));
        if (it == vars.end()) return std::nullopt;
        return it->second;
    };
}

std::map<std::string, std::string> valid_vars() {
    return {{"DATABASE_URL", "postgresql://app:hunter2@db:5432/orders"},
            {"API_TOKEN", "0123456789abcdef-secret"}};
}

std::vector<std::string> config_errors(const std::map<std::string, std::string>& vars) {
    try {
        (void)dep::Config::from_env(env_from(vars));
    } catch (const dep::ConfigError& e) {
        return e.errors();
    }
    return {"<no ConfigError>"};
}

std::string dotenv_error(std::string_view text) {
    auto r = dep::load_dotenv(text);
    if (r.has_value()) return "<accepted>";
    return r.error();
}

}  // namespace

TEST(Part1Config, ParseBoolAcceptsTrueWords) {
    for (auto word : {"1", "true", "YES", "On", " on "}) {
        auto r = dep::parse_bool(word);
        ASSERT_TRUE(r.has_value()) << word;
        EXPECT_TRUE(*r) << word;
    }
}

TEST(Part1Config, ParseBoolAcceptsFalseWords) {
    for (auto word : {"0", "False", "no", "OFF"}) {
        auto r = dep::parse_bool(word);
        ASSERT_TRUE(r.has_value()) << word;
        EXPECT_FALSE(*r) << word;
    }
}

TEST(Part1Config, ParseBoolRejectsOtherText) {
    EXPECT_FALSE(dep::parse_bool("maybe").has_value());
    EXPECT_FALSE(dep::parse_bool("").has_value());
}

TEST(Part1Config, ParseDurationUnits) {
    EXPECT_EQ(dep::parse_duration("500ms").value(), 500ms);
    EXPECT_EQ(dep::parse_duration("30s").value(), 30s);
    EXPECT_EQ(dep::parse_duration("5m").value(), 5min);
    EXPECT_EQ(dep::parse_duration("1h").value(), 1h);
    EXPECT_EQ(dep::parse_duration("2").value(), 2s);
    EXPECT_EQ(dep::parse_duration("1.5s").value(), 1500ms);
}

TEST(Part1Config, ParseDurationRejectsBadInput) {
    for (auto text : {"", "-1s", "5 minutes", "3d", "abc", "1.2.3s"}) {
        EXPECT_FALSE(dep::parse_duration(text).has_value()) << text;
    }
}

TEST(Part1Config, ParseListStripsAndDropsEmptyItems) {
    EXPECT_EQ(dep::parse_list("a, b,,  c ,"), (std::vector<std::string>{"a", "b", "c"}));
    EXPECT_TRUE(dep::parse_list("").empty());
}

TEST(Part1Config, MaskUrlPasswordHidesOnlyThePassword) {
    EXPECT_EQ(dep::mask_url_password("postgresql://app:hunter2@db:5432/x"), "postgresql://app:***@db:5432/x");
    EXPECT_EQ(dep::mask_url_password("postgresql://app:p@ss@db/x"), "postgresql://app:***@db/x");
    EXPECT_EQ(dep::mask_url_password("sqlite:///tmp/x.db"), "sqlite:///tmp/x.db");
    EXPECT_EQ(dep::mask_url_password("postgresql://user@db/x"), "postgresql://user@db/x");
    EXPECT_EQ(dep::mask_url_password("https://h/x@y"), "https://h/x@y");
}

TEST(Part1Config, DefaultsApplyWhenOptionalVariablesAreAbsent) {
    const auto cfg = dep::Config::from_env(env_from(valid_vars()));
    EXPECT_EQ(cfg.database_url, "postgresql://app:hunter2@db:5432/orders");
    EXPECT_EQ(cfg.app_name, "cppref-service");
    EXPECT_EQ(cfg.port, 8080);
    EXPECT_FALSE(cfg.debug);
    EXPECT_TRUE(cfg.allowed_hosts.empty());
    EXPECT_EQ(cfg.request_timeout, 30s);
    EXPECT_EQ(cfg.log_level, dep::LogLevel::info);
    EXPECT_FALSE(cfg.git_sha.has_value());
}

TEST(Part1Config, EveryVariableIsParsed) {
    auto vars = valid_vars();
    vars["APP_NAME"] = "orders";
    vars["PORT"] = " 9000 ";
    vars["DEBUG"] = "yes";
    vars["ALLOWED_HOSTS"] = "a.example, b.example";
    vars["REQUEST_TIMEOUT"] = "2m";
    vars["LOG_LEVEL"] = "warning";
    vars["GIT_SHA"] = "ABCDEF1";
    const auto cfg = dep::Config::from_env(env_from(vars));
    EXPECT_EQ(cfg.app_name, "orders");
    EXPECT_EQ(cfg.port, 9000);
    EXPECT_TRUE(cfg.debug);
    EXPECT_EQ(cfg.allowed_hosts, (std::vector<std::string>{"a.example", "b.example"}));
    EXPECT_EQ(cfg.request_timeout, 120s);
    EXPECT_EQ(cfg.log_level, dep::LogLevel::warning);
    ASSERT_TRUE(cfg.git_sha.has_value());
    EXPECT_EQ(*cfg.git_sha, "abcdef1");
}

TEST(Part1Config, EmptyOrBlankValueCountsAsUnset) {
    auto vars = valid_vars();
    vars["APP_NAME"] = "";
    vars["PORT"] = "   ";
    const auto cfg = dep::Config::from_env(env_from(vars));
    EXPECT_EQ(cfg.app_name, "cppref-service");
    EXPECT_EQ(cfg.port, 8080);

    auto blank_token = valid_vars();
    blank_token["API_TOKEN"] = "   ";
    EXPECT_EQ(config_errors(blank_token), (std::vector<std::string>{"API_TOKEN: required but not set"}));
}

TEST(Part1Config, ReportsEveryMissingRequiredVariable) {
    EXPECT_EQ(config_errors({}), (std::vector<std::string>{"DATABASE_URL: required but not set",
                                                           "API_TOKEN: required but not set"}));
}

TEST(Part1Config, ConfigErrorWhatJoinsEntries) {
    try {
        (void)dep::Config::from_env(env_from({}));
        ADD_FAILURE() << "expected ConfigError";
    } catch (const dep::ConfigError& e) {
        EXPECT_EQ(std::string(e.what()),
                  "invalid configuration: DATABASE_URL: required but not set; API_TOKEN: required but not set");
    }
}

TEST(Part1Config, BadValuesAreAllReportedAndSecretsStayOut) {
    const std::map<std::string, std::string> vars{
        {"DATABASE_URL", "mysql://leaky-pw@db/x"}, {"API_TOKEN", "tok-tiny"},
        {"PORT", "eighty"},                        {"DEBUG", "maybe"},
        {"REQUEST_TIMEOUT", "0s"},                 {"LOG_LEVEL", "loud"},
        {"GIT_SHA", "XYZ"}};
    const auto errors = config_errors(vars);
    ASSERT_EQ(errors.size(), 7u);
    for (const auto& e : errors) {
        EXPECT_EQ(e.find("leaky-pw"), std::string::npos) << e;
        EXPECT_EQ(e.find("tok-tiny"), std::string::npos) << e;
    }
    EXPECT_TRUE(errors[0].starts_with("DATABASE_URL:"));
}

TEST(Part1Config, LogLevelParsesCaseInsensitivelyAndPrintsUpper) {
    ASSERT_TRUE(dep::parse_log_level("Warning").has_value());
    EXPECT_EQ(*dep::parse_log_level("Warning"), dep::LogLevel::warning);
    EXPECT_EQ(dep::to_string(dep::LogLevel::critical), "CRITICAL");
    EXPECT_FALSE(dep::parse_log_level("loud").has_value());
}

TEST(Part1Config, RedactedDumpMasksSecrets) {
    const auto cfg = dep::Config::from_env(env_from(valid_vars()));
    const auto text = dep::redacted(cfg);
    EXPECT_NE(text.find("db:5432"), std::string::npos) << text;
    EXPECT_NE(text.find("app:***@"), std::string::npos) << text;
    EXPECT_EQ(text.find("hunter2"), std::string::npos) << text;
    EXPECT_EQ(text.find("0123456789abcdef-secret"), std::string::npos) << text;
}

TEST(Part1Config, DotenvGrammarBasics) {
    const std::string text = R"DOTENV(# comment

export A=1
B = 2
C='lit\n'
D="tab\there \"q\" back\\"
E=val # note
F=#only
A=3
)DOTENV";
    auto parsed = dep::load_dotenv(text);
    ASSERT_TRUE(parsed.has_value()) << parsed.error();
    const auto& m = *parsed;
    EXPECT_EQ(m.at("A"), "3");
    EXPECT_EQ(m.at("B"), "2");
    EXPECT_EQ(m.at("C"), "lit\\n");
    EXPECT_EQ(m.at("D"), "tab\there \"q\" back\\");
    EXPECT_EQ(m.at("E"), "val");
    EXPECT_EQ(m.at("F"), "");
}

TEST(Part1Config, DotenvErrorsNameTheLine) {
    EXPECT_TRUE(dotenv_error("NOEQUALS\n").starts_with("line 1: "));
    EXPECT_TRUE(dotenv_error("OK=1\nBAD KEY=1\n").starts_with("line 2: "));
    EXPECT_TRUE(dotenv_error("\nA=\"open\n").starts_with("line 2: "));
    EXPECT_TRUE(dotenv_error("A=\"x\" junk\n").starts_with("line 1: "));
    EXPECT_TRUE(dotenv_error("1BAD=2\n").starts_with("line 1: "));
}

TEST(Part1Config, RealEnvironmentBeatsDotenv) {
    dep::DotenvMap file{{"PORT", "2"}, {"APP_NAME", "from-file"}};
    const auto layered = dep::with_dotenv(env_from({{"PORT", "1"}}), file);
    EXPECT_EQ(layered("PORT"), "1");
    EXPECT_EQ(layered("APP_NAME"), "from-file");
    EXPECT_FALSE(layered("MISSING").has_value());
}

TEST(Part1Config, DotenvFileSuppliesRequiredValues) {
    dep::DotenvMap file{{"API_TOKEN", "0123456789abcdef-file"}, {"DATABASE_URL", "sqlite:/tmp/x.db"}};
    const auto cfg = dep::Config::from_env(dep::with_dotenv(env_from({}), file));
    EXPECT_EQ(cfg.api_token, "0123456789abcdef-file");
    EXPECT_EQ(cfg.database_url, "sqlite:/tmp/x.db");
}
