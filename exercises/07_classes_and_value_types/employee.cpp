#include "employee.hpp"

#include <stdexcept>
#include <string>

namespace cppref::classes {
namespace {

[[noreturn]] void not_implemented(const char* name) {
    throw std::logic_error(std::string("not implemented: ") + name);
}

}  // namespace

std::expected<Employee, EmployeeError> Employee::create(EmployeeFields) {
    not_implemented("Employee::create");
}

std::expected<Employee, EmployeeError> Employee::from_csv_row(std::span<const std::string_view>) {
    not_implemented("Employee::from_csv_row");
}

std::expected<std::string, EmployeeError> Employee::normalize_email(std::string_view) {
    not_implemented("Employee::normalize_email");
}

std::int64_t Employee::id() const {
    not_implemented("Employee::id");
}

std::string_view Employee::name() const {
    not_implemented("Employee::name");
}

std::string_view Employee::email() const {
    not_implemented("Employee::email");
}

std::string_view Employee::department() const {
    not_implemented("Employee::department");
}

const Money& Employee::salary() const {
    not_implemented("Employee::salary");
}

const std::vector<std::string>& Employee::skills() const {
    not_implemented("Employee::skills");
}

bool operator==(const Employee&, const Employee&) {
    not_implemented("operator==(Employee, Employee)");
}

}  // namespace cppref::classes
