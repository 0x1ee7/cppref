#include "validation.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace fn = cppref::functional;
using fn::Errors;
using fn::Validator;
using fn::Value;

namespace {

Value v_null() { return Value{}; }
Value v_bool(bool b) { return Value{b}; }
Value v_int(std::int64_t i) { return Value{i}; }
Value v_real(double d) { return Value{d}; }
Value v_str(std::string s) { return Value{std::move(s)}; }
Value v_list(fn::Array a) { return Value{std::move(a)}; }
Value v_map(fn::Object o) { return Value{std::move(o)}; }

Value signup(Value username, Value age, Value email, std::optional<Value> tags = std::nullopt) {
    fn::Object o{{"username", std::move(username)}, {"age", std::move(age)}, {"email", std::move(email)}};
    if (tags) {
        o.emplace_back("tags", std::move(*tags));
    }
    return v_map(std::move(o));
}

}  // namespace

TEST(Part6Validators, IsTypeNamesTheMismatchAndKeepsBoolOutOfInt) {
    EXPECT_EQ(fn::is_type(fn::ValueKind::integer)(v_bool(true)),
              (Errors{"expected int, got bool"}));
    EXPECT_TRUE(fn::is_type(fn::ValueKind::integer)(v_int(1)).empty());
    EXPECT_EQ(fn::is_type(fn::ValueKind::integer)(v_real(1.5)), (Errors{"expected int, got float"}));
    EXPECT_EQ(fn::is_type(fn::ValueKind::real)(v_int(2)), (Errors{"expected float, got int"}));
}

TEST(Part6Validators, MinLengthAndMatchesOnlyJudgeStrings) {
    EXPECT_EQ(fn::min_length(3)(v_str("ab")), (Errors{"length must be at least 3"}));
    EXPECT_TRUE(fn::min_length(3)(v_int(5)).empty());
    EXPECT_EQ(fn::matches("[a-z]+")(v_str("ABC")), (Errors{"must match pattern '[a-z]+'"}));
    EXPECT_TRUE(fn::matches("[a-z]+")(v_null()).empty());
}

TEST(Part6Validators, InRangeIsInclusiveAndIgnoresOtherKinds) {
    EXPECT_TRUE(fn::in_range(1, 10)(v_int(1)).empty());
    EXPECT_TRUE(fn::in_range(1, 10)(v_int(10)).empty());
    EXPECT_EQ(fn::in_range(1, 10)(v_int(11)), (Errors{"must be between 1 and 10"}));
    EXPECT_EQ(fn::in_range(1, 10)(v_real(10.5)), (Errors{"must be between 1 and 10"}));
    EXPECT_TRUE(fn::in_range(1, 10)(v_str("99")).empty());
}

TEST(Part6Validators, AllOfCollectsEveryError) {
    auto v = fn::all_of({fn::min_length(5), fn::matches("[0-9]+")});
    EXPECT_EQ(v(v_str("ab")), (Errors{"length must be at least 5", "must match pattern '[0-9]+'"}));
}

TEST(Part6Validators, AnyOfShortCircuitsOnFirstPass) {
    int calls = 0;
    Validator passes = [](const Value&) { return Errors{}; };
    Validator counted = [&calls](const Value&) {
        ++calls;
        return Errors{};
    };
    auto v = fn::any_of({fn::min_length(10), passes, counted});
    EXPECT_TRUE(v(v_str("x")).empty());
    EXPECT_EQ(calls, 0);
}

TEST(Part6Validators, AnyOfReturnsAllErrorsWhenNonePass) {
    auto v = fn::any_of({fn::min_length(5), fn::matches("[0-9]+")});
    EXPECT_EQ(v(v_str("ab")).size(), 2u);
}

TEST(Part6Validators, AnyOfWithoutValidatorsThrows) {
    EXPECT_THROW((void)fn::any_of({}), std::invalid_argument);
}

TEST(Part6Validators, FieldPrefixesErrorsAndReportsMissing) {
    auto obj = v_map({{"username", v_str("A")}});
    EXPECT_EQ(fn::field("username", fn::min_length(3))(obj),
              (Errors{"username: length must be at least 3"}));
    EXPECT_EQ(fn::field("email", fn::min_length(1))(obj), (Errors{"email: missing"}));
    EXPECT_TRUE(fn::field("username", fn::min_length(3))(v_str("x")).empty());
}

TEST(Part6Validators, OptionalAcceptsNullAndEachIndexesErrors) {
    EXPECT_TRUE(fn::optional(fn::min_length(3))(v_null()).empty());
    auto items = v_list({v_str("ok"), v_str("x"), v_str("fine")});
    EXPECT_EQ(fn::each(fn::min_length(2))(items), (Errors{"[1] length must be at least 2"}));
    EXPECT_TRUE(fn::each(fn::min_length(2))(v_str("x")).empty());
}

TEST(Part6Validators, ValidateSignupAcceptsGoodPayloads) {
    EXPECT_TRUE(fn::validate_signup(signup(v_str("ada_01"), v_int(36), v_str("ada@example.com"),
                                           v_list({v_str("admin"), v_str("staff")})))
                    .empty());
    // tags is optional
    EXPECT_TRUE(fn::validate_signup(signup(v_str("ada_01"), v_int(36), v_str("ada@example.com"))).empty());
}

TEST(Part6Validators, ValidateSignupReportsEveryProblemInOrder) {
    auto payload = signup(v_str("A"), v_int(7), v_str("bad"), v_list({v_str("")}));
    EXPECT_EQ(fn::validate_signup(payload),
              (Errors{
                  "username: length must be at least 3",
                  "username: must match pattern '[a-z0-9_]+'",
                  "age: must be between 13 and 120",
                  "email: must match pattern '[^@\\s]+@[^@\\s]+\\.[^@\\s]+'",
                  "tags: [0] length must be at least 1",
              }));
}

TEST(Part6Validators, ValidateSignupRejectsNonMapping) {
    EXPECT_EQ(fn::validate_signup(v_list({})), (Errors{"expected Mapping, got list"}));
}
