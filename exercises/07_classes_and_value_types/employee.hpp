// Part 6: Employee — validated value type with factories (aggregate input, CSV row).
#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "money.hpp"

namespace cppref::classes {

enum class EmployeeError {
    invalid_id,        ///< id <= 0, or CSV id is not a base-10 integer
    invalid_name,      ///< blank after trimming
    invalid_email,     ///< see Employee::normalize_email
    invalid_department,///< blank after trimming
    invalid_salary,    ///< negative, or CSV salary is not a valid amount
    invalid_currency,  ///< CSV currency code is invalid
    wrong_column_count,///< CSV row does not have exactly 7 columns
};

/// Plain input bundle for Employee::create(). Nothing here is trusted until create() runs.
struct EmployeeFields {
    std::int64_t id;
    std::string name;
    std::string email;
    std::string department;
    Money salary;
    std::vector<std::string> skills;
};

class Employee {
public:
    /// Validates every field; the only way to build an Employee.
    [[nodiscard]] static std::expected<Employee, EmployeeError> create(EmployeeFields fields);

    /// Columns: employee_id, name, email, department, salary, currency, skills.
    /// skills is ';'-separated and may be empty. Any other column count is wrong_column_count.
    [[nodiscard]] static std::expected<Employee, EmployeeError> from_csv_row(std::span<const std::string_view> row);

    /// Trim and lowercase. Exactly one '@', non-empty local part and domain, no whitespace anywhere.
    [[nodiscard]] static std::expected<std::string, EmployeeError> normalize_email(std::string_view raw);

    [[nodiscard]] std::int64_t id() const;
    [[nodiscard]] std::string_view name() const;
    [[nodiscard]] std::string_view email() const;
    [[nodiscard]] std::string_view department() const;
    [[nodiscard]] const Money& salary() const;
    [[nodiscard]] const std::vector<std::string>& skills() const;

    /// Field-by-field equality.
    friend bool operator==(const Employee& lhs, const Employee& rhs);

    // engineer: private constructor and data members
};

}  // namespace cppref::classes
