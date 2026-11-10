// Part 6: Employee — validated value type built by factories (aggregate input and CSV row).
#include <gtest/gtest.h>

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "employee.hpp"

namespace cc = cppref::classes;

namespace {

cc::Money usd(std::string_view text) {
    return cc::Money::parse(text, cc::Currency::parse("USD").value()).value();
}

cc::EmployeeFields valid_fields() {
    return cc::EmployeeFields{
        .id = 7,
        .name = "  Ada Lovelace ",
        .email = " Ada@Example.COM ",
        .department = "R&D",
        .salary = usd("85000.00"),
        .skills = {"C++", "c++", " Rust ", ""},
    };
}

template <typename T, typename E>
void expect_error(const std::expected<T, E>& result, E expected) {
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), expected);
}

}  // namespace

TEST(Part6Employee, CreateNormalisesFields) {
    auto employee = cc::Employee::create(valid_fields());
    ASSERT_TRUE(employee.has_value());
    EXPECT_EQ(employee->id(), 7);
    EXPECT_EQ(employee->name(), "Ada Lovelace");
    EXPECT_EQ(employee->email(), "ada@example.com");
    EXPECT_EQ(employee->department(), "R&D");
    EXPECT_EQ(employee->salary(), usd("85000.00"));
    EXPECT_EQ(employee->skills(), (std::vector<std::string>{"c++", "rust"}));
}

TEST(Part6Employee, CreateRejectsInvalidScalars) {
    auto bad_id = valid_fields();
    bad_id.id = 0;
    expect_error(cc::Employee::create(bad_id), cc::EmployeeError::invalid_id);

    auto blank_name = valid_fields();
    blank_name.name = "   ";
    expect_error(cc::Employee::create(blank_name), cc::EmployeeError::invalid_name);

    auto blank_dept = valid_fields();
    blank_dept.department = "";
    expect_error(cc::Employee::create(blank_dept), cc::EmployeeError::invalid_department);

    auto negative = valid_fields();
    negative.salary = usd("-1.00");
    expect_error(cc::Employee::create(negative), cc::EmployeeError::invalid_salary);
}

TEST(Part6Employee, NormalizeEmailRules) {
    auto ok = cc::Employee::normalize_email("  X@Y.ORG ");
    ASSERT_TRUE(ok.has_value());
    EXPECT_EQ(*ok, "x@y.org");

    for (std::string_view bad : {"a@@b", "no-at-sign", "a@", "@b", "a b@c.d"}) {
        expect_error(cc::Employee::normalize_email(bad), cc::EmployeeError::invalid_email);
    }
}

TEST(Part6Employee, FromCsvRowParsesAllColumns) {
    const std::array<std::string_view, 7> row{"42", "Grace Hopper", "grace@navy.mil", "Compilers",
                                              "120000.50", "USD", "COBOL; C ;cobol"};
    auto employee = cc::Employee::from_csv_row(std::span<const std::string_view>(row));
    ASSERT_TRUE(employee.has_value());
    EXPECT_EQ(employee->id(), 42);
    EXPECT_EQ(employee->salary(), usd("120000.50"));
    EXPECT_EQ(employee->skills(), (std::vector<std::string>{"cobol", "c"}));
}

TEST(Part6Employee, FromCsvRowEmptySkillsColumnAndEquality) {
    const std::array<std::string_view, 7> row{"5", "Alan", "alan@bletchley.uk", "Logic", "1.00", "USD", ""};
    auto first = cc::Employee::from_csv_row(std::span<const std::string_view>(row));
    ASSERT_TRUE(first.has_value());
    EXPECT_TRUE(first->skills().empty());

    auto second = cc::Employee::from_csv_row(std::span<const std::string_view>(row));
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*first, *second);
}

TEST(Part6Employee, FromCsvRowRejectsBadColumns) {
    const std::array<std::string_view, 6> short_row{"1", "A", "a@b.co", "D", "1.00", "USD"};
    expect_error(cc::Employee::from_csv_row(std::span<const std::string_view>(short_row)),
                 cc::EmployeeError::wrong_column_count);

    const std::array<std::string_view, 7> bad_id{"4x2", "A", "a@b.co", "D", "1.00", "USD", ""};
    expect_error(cc::Employee::from_csv_row(std::span<const std::string_view>(bad_id)),
                 cc::EmployeeError::invalid_id);

    const std::array<std::string_view, 7> bad_salary{"4", "A", "a@b.co", "D", "12,5", "USD", ""};
    expect_error(cc::Employee::from_csv_row(std::span<const std::string_view>(bad_salary)),
                 cc::EmployeeError::invalid_salary);

    const std::array<std::string_view, 7> bad_currency{"4", "A", "a@b.co", "D", "1.00", "usd", ""};
    expect_error(cc::Employee::from_csv_row(std::span<const std::string_view>(bad_currency)),
                 cc::EmployeeError::invalid_currency);
}
