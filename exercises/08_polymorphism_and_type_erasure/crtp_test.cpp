// Part 3: CRTP mixins (Comparable, Printable, InstanceCounter).
// Part 4: CRTP model validation with typed Field<T> members.
#include <gtest/gtest.h>

#include <compare>
#include <cstddef>
#include <expected>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

#include "crtp.hpp"

namespace poly = cppref::polymorphism;

namespace {

// ---- Part 3 fixtures -------------------------------------------------------

struct Version : poly::Comparable<Version>, poly::Printable<Version> {
    int major;
    int minor;

    Version(int ma, int mi) : major(ma), minor(mi) {}

    [[nodiscard]] std::tuple<int, int> sort_key() const { return {major, minor}; }
    [[nodiscard]] std::string describe() const {
        return std::to_string(major) + "." + std::to_string(minor);
    }
};

struct Widget : poly::InstanceCounter<Widget> {};
struct Gadget : poly::InstanceCounter<Gadget> {};

// ---- Part 4 fixtures -------------------------------------------------------

std::expected<void, std::string> non_empty(const std::string& s) {
    if (s.empty()) {
        return std::unexpected(std::string("must not be empty"));
    }
    return {};
}

std::expected<void, std::string> non_negative(const int& v) {
    if (v < 0) {
        return std::unexpected(std::string("must be >= 0"));
    }
    return {};
}

std::expected<void, std::string> at_most_150(const int& v) {
    if (v > 150) {
        return std::unexpected(std::string("must be <= 150"));
    }
    return {};
}

struct Account : poly::Model<Account> {
    poly::Field<std::string> owner{"owner", "", {non_empty}};
    poly::Field<int> balance{"balance", 0, {non_negative}};
    poly::Field<int> limit{"limit", 100, {at_most_150}};

    static constexpr auto schema = std::tuple{&Account::owner, &Account::balance, &Account::limit};
};

// `hi` starts invalid on purpose, to exercise validate() on an unvalidated initial value.
struct Pair : poly::Model<Pair> {
    poly::Field<int> lo{"lo", 0, {non_negative}};
    poly::Field<int> hi{"hi", -1, {non_negative}};

    static constexpr auto schema = std::tuple{&Pair::lo, &Pair::hi};
};

}  // namespace

// ---- Part 3 ----------------------------------------------------------------

TEST(Part3Mixins, EqualityAndOrderingComeFromSortKey) {
    const Version a{1, 2};
    const Version b{1, 2};
    const Version c{1, 10};
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);
    EXPECT_TRUE(a < c);
    EXPECT_TRUE(c > a);
    EXPECT_TRUE(a <= b);
    EXPECT_TRUE(c >= b);
}

TEST(Part3Mixins, ThreeWayComparisonIsStrongOrdering) {
    static_assert(std::is_same_v<decltype(Version{1, 0} <=> Version{2, 0}), std::strong_ordering>);
    const Version lo{1, 0};
    const Version hi{2, 0};
    EXPECT_TRUE((lo <=> hi) == std::strong_ordering::less);
    EXPECT_TRUE((hi <=> lo) == std::strong_ordering::greater);
    EXPECT_TRUE((lo <=> Version{1, 0}) == std::strong_ordering::equal);
}

TEST(Part3Mixins, ToStringAndStreamUseDescribe) {
    const Version v{1, 4};
    EXPECT_EQ(v.to_string(), "1.4");
    std::ostringstream os;
    os << v;
    EXPECT_EQ(os.str(), "1.4");
}

TEST(Part3Mixins, InstanceCounterTracksEachDerivedTypeSeparately) {
    const std::size_t widgets = Widget::live();
    const std::size_t gadgets = Gadget::live();
    {
        Widget a;
        Widget b = a;
        Widget c = std::move(a);
        Gadget g;
        EXPECT_EQ(Widget::live(), widgets + 3);
        EXPECT_EQ(Gadget::live(), gadgets + 1);
    }
    EXPECT_EQ(Widget::live(), widgets);
    EXPECT_EQ(Gadget::live(), gadgets);
}

TEST(Part3Mixins, CopyAssignmentDoesNotChangeLiveCount) {
    const std::size_t base = Widget::live();
    Widget a;
    Widget b;
    EXPECT_EQ(Widget::live(), base + 2);
    b = a;
    EXPECT_EQ(Widget::live(), base + 2);
}

// ---- Part 4 ----------------------------------------------------------------

TEST(Part4Fields, InitialValueIsStoredWithoutValidation) {
    const poly::Field<int> f{"retries", -5, {non_negative}};
    EXPECT_EQ(f.name(), "retries");
    EXPECT_EQ(f.get(), -5);
}

TEST(Part4Fields, SetStoresValidValue) {
    poly::Field<int> f{"n", 0, {non_negative, at_most_150}};
    const auto r = f.set(42);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(f.get(), 42);
}

TEST(Part4Fields, FailedSetReportsReasonAndKeepsStoredValue) {
    poly::Field<int> f{"n", 3, {non_negative, at_most_150}};
    const auto neg = f.set(-1);
    ASSERT_FALSE(neg.has_value());
    EXPECT_EQ(neg.error(), "must be >= 0");
    const auto big = f.set(200);
    ASSERT_FALSE(big.has_value());
    EXPECT_EQ(big.error(), "must be <= 150");
    EXPECT_EQ(f.get(), 3);
}

TEST(Part4Fields, RulesRunInOrderAndFirstFailureWins) {
    int first_calls = 0;
    int second_calls = 0;
    poly::Field<int>::Rule fail_first = [&first_calls](const int&) -> std::expected<void, std::string> {
        ++first_calls;
        return std::unexpected(std::string("first"));
    };
    poly::Field<int>::Rule fail_second = [&second_calls](const int&) -> std::expected<void, std::string> {
        ++second_calls;
        return std::unexpected(std::string("second"));
    };
    poly::Field<int> f{"n", 0, {fail_first, fail_second}};
    const auto r = f.set(1);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), "first");
    EXPECT_EQ(first_calls, 1);
    EXPECT_EQ(second_calls, 0);
}

TEST(Part4Fields, ValidateChecksTheStoredValue) {
    poly::Field<int> f{"n", -1, {non_negative}};
    const auto before = f.validate();
    ASSERT_FALSE(before.has_value());
    EXPECT_EQ(before.error(), "must be >= 0");
    ASSERT_TRUE(f.set(1).has_value());
    EXPECT_TRUE(f.validate().has_value());
}

TEST(Part4Fields, ModelReportsFirstFailingFieldWithPrefix) {
    Account a;
    const auto r = a.validate();
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), "owner: must not be empty");
}

TEST(Part4Fields, ModelSucceedsWhenEveryFieldIsValid) {
    Account a;
    ASSERT_TRUE(a.owner.set("ann").has_value());
    EXPECT_TRUE(a.validate().has_value());
}

TEST(Part4Fields, ModelReportsLaterFieldWhenEarlierFieldsAreValid) {
    Pair p;
    const auto r = p.validate();
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), "hi: must be >= 0");
}

TEST(Part4Fields, LimitRuleRejectsValuesAboveCap) {
    Account a;
    const auto r = a.limit.set(151);
    ASSERT_FALSE(r.has_value());
    EXPECT_EQ(r.error(), "must be <= 150");
    EXPECT_EQ(a.limit.get(), 100);
}

TEST(Part4Fields, ModelFieldCountComesFromSchema) {
    EXPECT_EQ(Account::field_count(), 3U);
    EXPECT_EQ(Pair::field_count(), 2U);
}
