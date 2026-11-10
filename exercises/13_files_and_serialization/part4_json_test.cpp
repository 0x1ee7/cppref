// Part 4: minimal JSON (json.hpp) and manifest JSON persistence (manifest_store.hpp).
#include "json.hpp"
#include "manifest_store.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

using cppref::files::FileEntry;
using cppref::files::Json;
using cppref::files::JsonError;
using cppref::files::Manifest;
using cppref::files::manifest_from_json;
using cppref::files::manifest_to_json;

namespace {

std::string nested_arrays(std::size_t depth) {
    return std::string(depth, '[') + std::string(depth, ']');
}

}  // namespace

TEST(Part4Json, ParsesScalarsAndContainers) {
    const Json doc = Json::parse(R"({"a":[1,true,null,"x"],"b":{}})");
    ASSERT_EQ(doc.kind(), Json::Kind::object);
    const Json* a = doc.find("a");
    ASSERT_NE(a, nullptr);
    ASSERT_EQ(a->kind(), Json::Kind::array);
    ASSERT_EQ(a->as_array().size(), 4u);
    EXPECT_EQ(a->as_array()[0].as_integer(), 1);
    EXPECT_TRUE(a->as_array()[1].as_bool());
    EXPECT_EQ(a->as_array()[2].kind(), Json::Kind::null);
    EXPECT_EQ(a->as_array()[3].as_string(), "x");
    EXPECT_EQ(doc.find("b")->kind(), Json::Kind::object);
}

TEST(Part4Json, DumpCompactSortsObjectKeys) {
    const Json doc = Json::object({{"b", Json::integer(1)}, {"a", Json::integer(2)}});
    EXPECT_EQ(doc.dump(), R"({"a":2,"b":1})");
}

TEST(Part4Json, DumpIndentedLayout) {
    const Json doc = Json::object({
        {"a", Json::array({Json::integer(1), Json::integer(2)})},
        {"b", Json::object({})},
    });
    EXPECT_EQ(doc.dump(2), "{\n  \"a\": [\n    1,\n    2\n  ],\n  \"b\": {}\n}");
}

TEST(Part4Json, DumpEscapesQuotesBackslashesAndControls) {
    const Json s = Json::string(std::string("q\"\\n\n\t\x01", 7));
    EXPECT_EQ(s.dump(), "\"q\\\"\\\\n\\n\\t\\u0001\"");
}

TEST(Part4Json, ParseDecodesUnicodeEscapesIncludingSurrogatePairs) {
    const Json s = Json::parse("\"\\u00e9\\ud83d\\ude00\"");
    EXPECT_EQ(s.as_string(), "\xc3\xa9\xf0\x9f\x98\x80");
}

TEST(Part4Json, NonAsciiUtf8PassesThroughUnchanged) {
    const std::string text = "\"caf\xc3\xa9 \xe2\x82\xac\"";
    EXPECT_EQ(Json::parse(text).dump(), text);
}

TEST(Part4Json, IntegersRoundTripAcrossInt64Range) {
    EXPECT_EQ(Json::parse("9223372036854775807").as_integer(), std::numeric_limits<std::int64_t>::max());
    EXPECT_EQ(Json::parse("-9223372036854775808").as_integer(), std::numeric_limits<std::int64_t>::min());
    EXPECT_THROW(static_cast<void>(Json::parse("9223372036854775808")), JsonError);
}

TEST(Part4Json, RejectsFractionsExponentsAndLeadingZeros) {
    for (const char* bad : {"1.5", "1e3", "1E3", "-0.0", "01", "-01"}) {
        SCOPED_TRACE(bad);
        EXPECT_THROW(static_cast<void>(Json::parse(bad)), JsonError);
    }
}

TEST(Part4Json, RejectsEmptyInputTrailingContentAndTrailingCommas) {
    for (const char* bad : {"", "   ", "1 2", "[1,]", R"({"a":1,})"}) {
        SCOPED_TRACE(bad);
        EXPECT_THROW(static_cast<void>(Json::parse(bad)), JsonError);
    }
}

TEST(Part4Json, RejectsDuplicateKeys) {
    EXPECT_THROW(static_cast<void>(Json::parse(R"({"k":1,"k":2})")), JsonError);
    EXPECT_THROW(Json::object({{"k", Json::integer(1)}, {"k", Json::integer(2)}}), JsonError);
}

TEST(Part4Json, RejectsRawControlCharactersAndBadEscapes) {
    EXPECT_THROW(static_cast<void>(Json::parse(std::string("\"a\x01" "b\"", 5))), JsonError);
    EXPECT_THROW(static_cast<void>(Json::parse(R"("\x41")")), JsonError);
}

TEST(Part4Json, RejectsLoneSurrogates) {
    EXPECT_THROW(static_cast<void>(Json::parse(R"("\ud83d")")), JsonError);
    EXPECT_THROW(static_cast<void>(Json::parse(R"("\ude00")")), JsonError);
}

TEST(Part4Json, NestingDepthLimitIs128) {
    EXPECT_NO_THROW(static_cast<void>(Json::parse(nested_arrays(128))));
    EXPECT_THROW(static_cast<void>(Json::parse(nested_arrays(129))), JsonError);
}

TEST(Part4Json, ErrorOffsetPointsAtOffendingCharacter) {
    try {
        static_cast<void>(Json::parse("[1, x]"));
        FAIL() << "expected JsonError";
    } catch (const JsonError& e) {
        EXPECT_EQ(e.offset(), 4u);
    }
}

TEST(Part4Json, UnexpectedEndReportsInputSizeAsOffset) {
    try {
        static_cast<void>(Json::parse("[1,"));
        FAIL() << "expected JsonError";
    } catch (const JsonError& e) {
        EXPECT_EQ(e.offset(), 3u);
    }
}

TEST(Part4Json, AccessorKindMismatchThrows) {
    EXPECT_THROW(static_cast<void>(Json::integer(1).as_string()), JsonError);
    EXPECT_THROW(static_cast<void>(Json::string("s").as_integer()), JsonError);
    EXPECT_THROW(static_cast<void>(Json::null().find("k")), JsonError);
}

TEST(Part4Json, FindReturnsNullWhenKeyAbsent) {
    const Json doc = Json::object({{"present", Json::boolean(true)}});
    ASSERT_NE(doc.find("present"), nullptr);
    EXPECT_TRUE(doc.find("present")->as_bool());
    EXPECT_EQ(doc.find("absent"), nullptr);
}

TEST(Part4Json, DumpRejectsInvalidUtf8) {
    EXPECT_THROW(static_cast<void>(Json::string("\xff").dump()), JsonError);
}

TEST(Part4Manifest, RoundTripsThroughJson) {
    Manifest m{
        {"a.txt", FileEntry{3, cppref::files::Timestamp{std::chrono::microseconds{-5}}, std::string(64, 'a')}},
        {"dir/b.bin", FileEntry{0, cppref::files::Timestamp{std::chrono::microseconds{1'709'210'096'000'123}},
                                std::string(64, '0')}},
    };
    EXPECT_EQ(manifest_from_json(manifest_to_json(m)), m);
}

TEST(Part4Manifest, JsonDocumentHasFormatAndEntriesShape) {
    Manifest m{{"x.txt", FileEntry{7, cppref::files::Timestamp{std::chrono::microseconds{42}}, std::string(64, 'b')}}};
    const Json doc = Json::parse(manifest_to_json(m));
    ASSERT_NE(doc.find("format"), nullptr);
    EXPECT_EQ(doc.find("format")->as_integer(), 1);
    const Json* entries = doc.find("entries");
    ASSERT_NE(entries, nullptr);
    const Json* x = entries->find("x.txt");
    ASSERT_NE(x, nullptr);
    EXPECT_EQ(x->find("size")->as_integer(), 7);
    EXPECT_EQ(x->find("mtime_us")->as_integer(), 42);
    EXPECT_EQ(x->find("sha256")->as_string(), std::string(64, 'b'));
}

TEST(Part4Manifest, RejectsUnknownFormatVersion) {
    const std::string doc = R"({"format":2,"entries":{}})";
    EXPECT_THROW(static_cast<void>(manifest_from_json(doc)), JsonError);
}

TEST(Part4Manifest, RejectsMissingEntryField) {
    const std::string doc =
        R"({"format":1,"entries":{"a.txt":{"size":1,"sha256":")" + std::string(64, 'c') + R"("}}})";
    EXPECT_THROW(static_cast<void>(manifest_from_json(doc)), JsonError);
}

TEST(Part4Manifest, RejectsUppercaseOrShortDigestWhenLoading) {
    const std::string upper =
        R"({"format":1,"entries":{"a.txt":{"size":1,"mtime_us":0,"sha256":")" + std::string(64, 'A') + R"("}}})";
    EXPECT_THROW(static_cast<void>(manifest_from_json(upper)), JsonError);
}

TEST(Part4Manifest, RejectsUnsafeKeyWhenLoading) {
    const std::string doc =
        R"({"format":1,"entries":{"../x":{"size":1,"mtime_us":0,"sha256":")" + std::string(64, 'c') + R"("}}})";
    EXPECT_THROW(static_cast<void>(manifest_from_json(doc)), std::invalid_argument);
}

TEST(Part4Manifest, RejectsMalformedDigestWhenSaving) {
    Manifest m{{"a.txt", FileEntry{1, cppref::files::Timestamp{}, "not-a-digest"}}};
    EXPECT_THROW(static_cast<void>(manifest_to_json(m)), std::invalid_argument);
}
