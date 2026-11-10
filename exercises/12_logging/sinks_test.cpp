// Part 3: text and JSON formatting, ostream sink, size-rotating file sink.
#include "sinks.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using cppref::logging::Field;
using cppref::logging::Level;
using cppref::logging::Record;
using cppref::logging::RotatingFileSink;
using cppref::logging::format_utc_iso8601;
using cppref::logging::json_escape;
using cppref::logging::json_format;
using cppref::logging::OstreamSink;
using cppref::logging::text_format;
using cppref::logging::testing::TempDir;
using cppref::logging::testing::fixed_time;
using cppref::logging::testing::make_record;
using cppref::logging::testing::read_file;
using cppref::logging::testing::split_lines;

// Streambuf that counts sync() calls, to observe flush().
class CountingBuf final : public std::stringbuf {
public:
    int syncs = 0;

protected:
    int sync() override {
        ++syncs;
        return std::stringbuf::sync();
    }
};

// Last token of a line: the message suffix "m012" used by the rotation tests.
std::string tail_token(const std::string& line) {
    return line.substr(line.size() - 4);
}

}  // namespace

// ---------------------------------------------------------------------------
// Part 3a: text and JSON formatting
// ---------------------------------------------------------------------------

TEST(Part3Formatting, Utc8601HasMillisecondsAndZ) {
    EXPECT_EQ(format_utc_iso8601(fixed_time()), "2023-11-14T22:13:20.123Z");
    EXPECT_EQ(format_utc_iso8601(std::chrono::system_clock::time_point{}),
              "1970-01-01T00:00:00.000Z");
}

TEST(Part3Formatting, TextFormatLayoutIsExact) {
    Record r = make_record(Level::info, "app.db", "hello", {Field{"k", "v"}});
    EXPECT_EQ(text_format(r), "2023-11-14T22:13:20.123Z INFO  app.db: hello k=v");
}

TEST(Part3Formatting, TextFormatWithoutFieldsHasNoTrailingSpace) {
    Record r = make_record(Level::error, "app", "boom");
    EXPECT_EQ(text_format(r), "2023-11-14T22:13:20.123Z ERROR app: boom");
}

TEST(Part3Formatting, JsonEscapeHandlesQuotesBackslashAndControls) {
    EXPECT_EQ(json_escape("a\"b\\c\nd\te\x01"), "a\\\"b\\\\c\\nd\\te\\u0001");
    EXPECT_EQ(json_escape("\b\f\r"), "\\b\\f\\r");
}

TEST(Part3Formatting, JsonEscapePassesUtf8Through) {
    EXPECT_EQ(json_escape("caf\xc3\xa9"), "caf\xc3\xa9");
}

TEST(Part3Formatting, JsonFormatIsSingleLineEvenWithNewlinesInMessage) {
    Record r = make_record(Level::info, "app", "line1\nline2");
    std::string json = json_format(r);
    EXPECT_EQ(json.find('\n'), std::string::npos);
    EXPECT_EQ(json.front(), '{');
    EXPECT_EQ(json.back(), '}');
    EXPECT_NE(json.find(R"("msg":"line1\nline2")"), std::string::npos);
}

TEST(Part3Formatting, JsonFormatHasCoreKeysAndLowercaseLevel) {
    Record r = make_record(Level::warn, "app.db", "hi");
    std::string json = json_format(r);
    EXPECT_NE(json.find(R"("ts":"2023-11-14T22:13:20.123Z")"), std::string::npos);
    EXPECT_NE(json.find(R"("level":"warn")"), std::string::npos);
    EXPECT_NE(json.find(R"("logger":"app.db")"), std::string::npos);
    EXPECT_NE(json.find(R"("msg":"hi")"), std::string::npos);
    EXPECT_NE(json.find(R"("file":")"), std::string::npos);
    EXPECT_NE(json.find(R"("line":)"), std::string::npos);
    EXPECT_NE(json.find(R"("fields":{})"), std::string::npos);
}

TEST(Part3Formatting, JsonFormatEscapesMessageQuotes) {
    Record r = make_record(Level::info, "app", R"(say "hi")");
    EXPECT_NE(json_format(r).find(R"("msg":"say \"hi\"")"), std::string::npos);
}

TEST(Part3Formatting, JsonFormatRendersFieldsAsObject) {
    Record r = make_record(Level::info, "app", "m", {Field{"k", "v\"x"}, Field{"n", "2"}});
    EXPECT_NE(json_format(r).find(R"("fields":{"k":"v\"x","n":"2"})"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Part 3b: ostream sink
// ---------------------------------------------------------------------------

TEST(Part3Ostream, WritesOneNewlineTerminatedLinePerRecord) {
    std::ostringstream out;
    OstreamSink sink(out);
    Record a = make_record(Level::info, "app", "first");
    Record b = make_record(Level::debug, "app", "second");

    sink.write(a);
    sink.write(b);

    EXPECT_EQ(out.str(), text_format(a) + "\n" + text_format(b) + "\n");
}

TEST(Part3Ostream, UsesTheSuppliedFormatter) {
    std::ostringstream out;
    OstreamSink sink(out, [](const Record& r) { return "custom:" + r.message; });
    sink.write(make_record(Level::info, "app", "x"));
    EXPECT_EQ(out.str(), "custom:x\n");
}

TEST(Part3Ostream, FlushSyncsTheUnderlyingStream) {
    CountingBuf buf;
    std::ostream out(&buf);
    OstreamSink sink(out);
    sink.write(make_record(Level::info, "app", "x"));

    sink.flush();

    EXPECT_GT(buf.syncs, 0);
}

TEST(Part3Ostream, JsonFormatterProducesOneObjectPerLine) {
    std::ostringstream out;
    OstreamSink sink(out, json_format);
    sink.write(make_record(Level::info, "app", "one"));
    sink.write(make_record(Level::info, "app", "two"));

    auto lines = split_lines(out.str());
    ASSERT_EQ(lines.size(), 2u);
    for (const auto& line : lines) {
        EXPECT_EQ(line.front(), '{');
        EXPECT_EQ(line.back(), '}');
    }
}

// ---------------------------------------------------------------------------
// Part 3c: size-rotating file sink
// ---------------------------------------------------------------------------

TEST(Part3Rotating, RejectsZeroLimits) {
    TempDir dir;
    EXPECT_THROW(RotatingFileSink(dir.path() / "a.log", 0, 1), std::invalid_argument);
    EXPECT_THROW(RotatingFileSink(dir.path() / "a.log", 100, 0), std::invalid_argument);
}

TEST(Part3Rotating, ExposesPathAndWritesLines) {
    TempDir dir;
    auto path = dir.path() / "app.log";
    RotatingFileSink sink(path, 10'000, 2);
    EXPECT_EQ(sink.path(), path);

    Record a = make_record(Level::info, "app", "alpha");
    Record b = make_record(Level::info, "app", "beta");
    sink.write(a);
    sink.write(b);
    sink.flush();

    EXPECT_EQ(read_file(path), text_format(a) + "\n" + text_format(b) + "\n");
}

TEST(Part3Rotating, AppendsToExistingFileAndCountsItsSize) {
    TempDir dir;
    auto path = dir.path() / "app.log";
    {
        std::ofstream(path, std::ios::binary) << "old\n";
    }
    RotatingFileSink sink(path, 10'000, 2);
    sink.write(make_record(Level::info, "app", "new"));
    sink.flush();

    EXPECT_TRUE(read_file(path).starts_with("old\n"));
}

TEST(Part3Rotating, RotatesWhenTheNextLineWouldExceedTheLimit) {
    TempDir dir;
    auto path = dir.path() / "app.log";
    // Each line is 41 bytes including '\n'; seven fit in 300 bytes, eight do not.
    RotatingFileSink sink(path, 300, 3);
    for (int i = 0; i < 30; ++i) {
        sink.write(make_record(Level::info, "app", std::format("m{:03}", i)));
    }
    sink.flush();

    EXPECT_TRUE(std::filesystem::exists(path));
    EXPECT_TRUE(std::filesystem::exists(dir.path() / "app.log.1"));
    EXPECT_TRUE(std::filesystem::exists(dir.path() / "app.log.2"));
    EXPECT_TRUE(std::filesystem::exists(dir.path() / "app.log.3"));
    EXPECT_FALSE(std::filesystem::exists(dir.path() / "app.log.4"));
    for (const auto& name : {"app.log", "app.log.1", "app.log.2", "app.log.3"}) {
        EXPECT_LE(std::filesystem::file_size(dir.path() / name), 300u) << name;
    }
}

TEST(Part3Rotating, KeepsOnlyTheMostRecentBackupsInOrder) {
    TempDir dir;
    auto path = dir.path() / "app.log";
    RotatingFileSink sink(path, 300, 3);
    for (int i = 0; i < 30; ++i) {
        sink.write(make_record(Level::info, "app", std::format("m{:03}", i)));
    }
    sink.flush();

    // Oldest surviving file first: .3, .2, .1, then the live file.
    std::vector<std::string> tokens;
    for (const auto& name : {"app.log.3", "app.log.2", "app.log.1", "app.log"}) {
        for (const auto& line : split_lines(read_file(dir.path() / name))) {
            tokens.push_back(tail_token(line));
        }
    }
    ASSERT_FALSE(tokens.empty());
    EXPECT_EQ(tokens.front(), "m007");
    EXPECT_EQ(tokens.back(), "m029");
    ASSERT_EQ(tokens.size(), 23u);
    for (std::size_t i = 0; i < tokens.size(); ++i) {
        EXPECT_EQ(tokens[i], std::format("m{:03}", i + 7));
    }
}

TEST(Part3Rotating, OversizedRecordIsWrittenNotDropped) {
    TempDir dir;
    auto path = dir.path() / "app.log";
    RotatingFileSink sink(path, 10, 2);  // every formatted line exceeds 10 bytes
    sink.write(make_record(Level::info, "app", "AAA"));
    sink.write(make_record(Level::info, "app", "BBB"));
    sink.flush();

    EXPECT_NE(read_file(dir.path() / "app.log.1").find("AAA"), std::string::npos);
    EXPECT_NE(read_file(path).find("BBB"), std::string::npos);
}

TEST(Part3Rotating, ExistingContentCountsTowardTheLimitAndIsRotated) {
    TempDir dir;
    auto path = dir.path() / "app.log";
    const std::string old_content(90, 'x');
    {
        std::ofstream(path, std::ios::binary) << old_content << "\n";
    }
    RotatingFileSink sink(path, 100, 1);
    sink.write(make_record(Level::info, "app", "fresh"));
    sink.flush();

    EXPECT_EQ(read_file(dir.path() / "app.log.1"), old_content + "\n");
    EXPECT_NE(read_file(path).find("fresh"), std::string::npos);
    EXPECT_FALSE(std::filesystem::exists(dir.path() / "app.log.2"));
}
