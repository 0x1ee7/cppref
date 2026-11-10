#include "amounts.hpp"
#include "config.hpp"
#include "errors.hpp"

#include <gtest/gtest.h>

#include <expected>
#include <filesystem>
#include <string>
#include <fstream>
#include <system_error>

namespace eh = cppref::error_handling;
namespace fs = std::filesystem;
using eh::LineItem;
using eh::LineItemError;
using eh::Money;
using eh::ParseFailure;

// --- Part 6: std::expected for recoverable parsing, and the config boundary --

TEST(Part6Expected, ParseAmountAcceptsWholeAndFractionalValues) {
    EXPECT_EQ(eh::parse_amount("12").value(), Money{1200});
    EXPECT_EQ(eh::parse_amount("12.3").value(), Money{1230});
    EXPECT_EQ(eh::parse_amount("12.34").value(), Money{1234});
    EXPECT_EQ(eh::parse_amount("0.05").value(), Money{5});
}

TEST(Part6Expected, ParseAmountRejectsEmptyAndNonNumeric) {
    ASSERT_FALSE(eh::parse_amount("").has_value());
    EXPECT_EQ(eh::parse_amount("").error(), ParseFailure::empty);
    EXPECT_FALSE(eh::parse_amount("-1").has_value());
    EXPECT_FALSE(eh::parse_amount("1,000").has_value());
    EXPECT_FALSE(eh::parse_amount("1e3").has_value());
    EXPECT_FALSE(eh::parse_amount("12.").has_value());
}

TEST(Part6Expected, ParseAmountRejectsMoreThanTwoDecimals) {
    ASSERT_FALSE(eh::parse_amount("1.234").has_value());
    EXPECT_EQ(eh::parse_amount("1.234").error(), ParseFailure::too_many_decimals);
}

TEST(Part6Expected, ParseAmountRejectsValuesThatDoNotFit) {
    ASSERT_FALSE(eh::parse_amount("99999999999999999999").has_value());
    EXPECT_EQ(eh::parse_amount("99999999999999999999").error(), ParseFailure::out_of_range);
}

TEST(Part6Expected, ParseQuantityIsPositiveAndInt) {
    EXPECT_EQ(eh::parse_quantity("3").value(), 3);
    EXPECT_EQ(eh::parse_quantity("0").error(), ParseFailure::out_of_range);
    EXPECT_EQ(eh::parse_quantity("abc").error(), ParseFailure::bad_character);
    EXPECT_EQ(eh::parse_quantity("").error(), ParseFailure::empty);
}

TEST(Part6Expected, ParseLineItemHappyPath) {
    const auto item = eh::parse_line_item("SKU-1,2,4.50");
    ASSERT_TRUE(item.has_value());
    EXPECT_EQ(*item, (LineItem{"SKU-1", 2, Money{450}}));
}

TEST(Part6Expected, ParseLineItemReportsTheFailingField) {
    EXPECT_EQ(eh::parse_line_item("SKU-1,x,1.00").error(), (LineItemError{1, ParseFailure::bad_character}));
    EXPECT_EQ(eh::parse_line_item("SKU-1,2,1.005").error(), (LineItemError{2, ParseFailure::too_many_decimals}));
    EXPECT_EQ(eh::parse_line_item("SKU-1,0,1.00").error(), (LineItemError{1, ParseFailure::out_of_range}));
}

TEST(Part6Expected, ParseLineItemReportsWrongFieldCount) {
    EXPECT_EQ(eh::parse_line_item("a,b").error(), (LineItemError{2, ParseFailure::wrong_field_count}));
    EXPECT_EQ(eh::parse_line_item("a,1,1.00,extra").error(), (LineItemError{4, ParseFailure::wrong_field_count}));
}

TEST(Part6Expected, TryLoadConfigMapsMissingFileToNotFound) {
    const auto result = eh::try_load_config(fs::temp_directory_path() / "cppref-ex10-no-such-file.ini");
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), eh::ConfigFailure::not_found);
}

TEST(Part6Expected, TryLoadConfigMapsSyntaxErrorToInvalid) {
    const auto file = fs::temp_directory_path() / "cppref-ex10-bad-syntax.ini";
    {
        std::ofstream out(file, std::ios::binary);
        out << "not a pair\n";
    }
    const auto result = eh::try_load_config(file);
    std::error_code ec;
    fs::remove(file, ec);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), eh::ConfigFailure::invalid);
}

TEST(Part6Expected, ValueOrThrowRaisesTheMatchingErrorWithPath) {
    const std::expected<eh::Config, eh::ConfigFailure> failed{std::unexpect, eh::ConfigFailure::not_found};
    try {
        (void)eh::value_or_throw(failed, "app.ini");
        FAIL() << "expected ConfigNotFound";
    } catch (const eh::ConfigNotFound& e) {
        EXPECT_NE(std::string(e.what()).find("app.ini"), std::string::npos);
    }
}

TEST(Part6Expected, ValueOrThrowReturnsTheConfigOnSuccess) {
    const std::expected<eh::Config, eh::ConfigFailure> ok{std::in_place, eh::Config{{"a.b", "1"}}};
    const auto cfg = eh::value_or_throw(ok, "app.ini");
    EXPECT_EQ(cfg.at("a.b"), "1");
}
