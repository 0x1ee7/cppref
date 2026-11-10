// Part 6: Employee — validated value type with factories (aggregate input, CSV row).
#pragma once

#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <string_view>
#include <utility>
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
    [[nodiscard]] static std::expected<Employee, EmployeeError> create(EmployeeFields fields);

    /// Columns: employee_id, name, email, department, salary, currency, skills.
    [[nodiscard]] static std::expected<Employee, EmployeeError> from_csv_row(std::span<const std::string_view> row);

    [[nodiscard]] static std::expected<std::string, EmployeeError> normalize_email(std::string_view raw);

    [[nodiscard]] std::int64_t id() const;
    [[nodiscard]] std::string_view name() const;
    [[nodiscard]] std::string_view email() const;
    [[nodiscard]] std::string_view department() const;
    [[nodiscard]] const Money& salary() const;
    [[nodiscard]] const std::vector<std::string>& skills() const;

    friend bool operator==(const Employee& lhs, const Employee& rhs);

private:
    Employee(std::int64_t id, std::string name, std::string email, std::string department, Money salary,
             std::vector<std::string> skills)
        : id_(id),
          name_(std::move(name)),
          email_(std::move(email)),
          department_(std::move(department)),
          salary_(std::move(salary)),
          skills_(std::move(skills)) {}

    std::int64_t id_;
    std::string name_;
    std::string email_;
    std::string department_;
    Money salary_;
    std::vector<std::string> skills_;
};

}  // namespace cppref::classes
