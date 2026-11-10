// Part 6: secret redaction in messages and structured fields, and the
// RedactingSink decorator that applies it before records reach the real sink.
#include "logging.hpp"
#include "redact.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

using cppref::logging::Field;
using cppref::logging::Level;
using cppref::logging::Registry;
using cppref::logging::RedactingSink;
using cppref::logging::Sink;
using cppref::logging::is_secret_key;
using cppref::logging::redact;
using cppref::logging::testing::CaptureSink;
using cppref::logging::testing::make_record;

}  // namespace

// ---------------------------------------------------------------------------
// Part 6: redaction rules
// ---------------------------------------------------------------------------

TEST(Part6Redact, MasksEqualsAndColonSeparatedSecrets) {
    EXPECT_EQ(redact("password=hunter2"), "password=***");
    EXPECT_EQ(redact("api_key: abc123"), "api_key: ***");
    EXPECT_EQ(redact("client_secret=xyz"), "client_secret=***");
}

TEST(Part6Redact, MatchesKeyFormsCaseInsensitively) {
    EXPECT_EQ(redact("PASSWORD=x"), "PASSWORD=***");
    EXPECT_EQ(redact("Api-Key=k1"), "Api-Key=***");
    EXPECT_EQ(redact("access_token=t1"), "access_token=***");
}

TEST(Part6Redact, PreservesQuotesOnQuotedValues) {
    EXPECT_EQ(redact(R"(token="abc def")"), R"(token="***")");
}

TEST(Part6Redact, UnquotedValueEndsAtDelimiters) {
    EXPECT_EQ(redact("password=abc&user=bob"), "password=***&user=bob");
    EXPECT_EQ(redact("password=abc, user=bob"), "password=***, user=bob");
    EXPECT_EQ(redact("password=abc;user=bob"), "password=***;user=bob");
}

TEST(Part6Redact, DoesNotTouchKeysThatOnlyContainASecretWord) {
    EXPECT_EQ(redact("passwordless=yes"), "passwordless=yes");
    EXPECT_EQ(redact("tokenizer=bpe"), "tokenizer=bpe");
    EXPECT_EQ(redact("my_password_hint=x"), "my_password_hint=x");
}

TEST(Part6Redact, LeavesOrdinaryTextUnchanged) {
    EXPECT_EQ(redact("user=bob status=ok"), "user=bob status=ok");
    EXPECT_EQ(redact(""), "");
}

TEST(Part6Redact, AuthorizationValueRunsToEndOfLine) {
    EXPECT_EQ(redact("authorization: Bearer abc.def"), "authorization: ***");
}

TEST(Part6Redact, HandlesPythonDictReprStyle) {
    EXPECT_EQ(redact("{'password': 'hunter2', 'user': 'bob'}"),
              "{'password': '***', 'user': 'bob'}");
}

TEST(Part6Redact, RedactsEveryOccurrenceInOneMessage) {
    EXPECT_EQ(redact("token=a password=b"), "token=*** password=***");
}

TEST(Part6Redact, SecretKeyPredicateMatchesWholeKeysOnly) {
    EXPECT_TRUE(is_secret_key("password"));
    EXPECT_TRUE(is_secret_key("API_KEY"));
    EXPECT_TRUE(is_secret_key("api-key"));
    EXPECT_TRUE(is_secret_key("Authorization"));
    EXPECT_FALSE(is_secret_key("user"));
    EXPECT_FALSE(is_secret_key("passwordless"));
}

// ---------------------------------------------------------------------------
// Part 6: RedactingSink
// ---------------------------------------------------------------------------

TEST(Part6RedactingSink, MasksMessageAndSecretFields) {
    auto target = std::make_shared<CaptureSink>();
    RedactingSink sink(target);

    sink.write(make_record(Level::info, "app", "login password=hunter2",
                           {Field{"token", "abc"}, Field{"user", "bob"}}));

    auto records = target->records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].message, "login password=***");
    EXPECT_EQ(records[0].fields, (std::vector<Field>{{"token", "***"}, {"user", "bob"}}));
}

TEST(Part6RedactingSink, NeverDropsRecords) {
    auto target = std::make_shared<CaptureSink>();
    RedactingSink sink(target);
    for (int i = 0; i < 3; ++i) {
        sink.write(make_record(Level::info, "app", "api_key=k"));
    }
    EXPECT_EQ(target->records().size(), 3u);
}

TEST(Part6RedactingSink, ForwardsFlush) {
    auto target = std::make_shared<CaptureSink>();
    RedactingSink sink(target);
    sink.flush();
    EXPECT_EQ(target->flush_count(), 1);
}

TEST(Part6RedactingSink, CoversChildLoggersThroughAncestorSink) {
    Registry reg;
    auto target = std::make_shared<CaptureSink>();
    reg.get("app").add_sink(std::make_shared<RedactingSink>(target));

    CPPREF_INFO(reg.get("app.db"), "connecting password={}", "hunter2");

    auto records = target->records();
    ASSERT_EQ(records.size(), 1u);
    EXPECT_EQ(records[0].message, "connecting password=***");
}
