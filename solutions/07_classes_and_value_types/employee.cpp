#include "employee.hpp"

#include <algorithm>
#include <charconv>
#include <system_error>

namespace cppref::classes {
namespace {

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && is_space(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_space(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

char to_lower(char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

std::string lowercase(std::string_view text) {
    std::string out(text);
    std::ranges::transform(out, out.begin(), to_lower);
    return out;
}

/// Trim, lowercase, drop blanks and duplicates (first occurrence wins).
std::vector<std::string> normalize_skills(const std::vector<std::string>& raw) {
    std::vector<std::string> out;
    for (const std::string& item : raw) {
        std::string skill = lowercase(trim(item));
        if (skill.empty()) {
            continue;
        }
        if (std::ranges::find(out, skill) == out.end()) {
            out.push_back(std::move(skill));
        }
    }
    return out;
}

}  // namespace

std::expected<std::string, EmployeeError> Employee::normalize_email(std::string_view raw) {
    const auto text = trim(raw);
    std::string email;
    email.reserve(text.size());
    for (char c : text) {
        if (is_space(c)) {
            return std::unexpected(EmployeeError::invalid_email);
        }
        email.push_back(to_lower(c));
    }
    const auto at = email.find('@');
    const bool well_formed = at != std::string::npos && at > 0 && at + 1 < email.size() &&
                             email.find('@', at + 1) == std::string::npos;
    if (!well_formed) {
        return std::unexpected(EmployeeError::invalid_email);
    }
    return email;
}

std::expected<Employee, EmployeeError> Employee::create(EmployeeFields fields) {
    if (fields.id <= 0) {
        return std::unexpected(EmployeeError::invalid_id);
    }
    const auto name = trim(fields.name);
    if (name.empty()) {
        return std::unexpected(EmployeeError::invalid_name);
    }
    const auto department = trim(fields.department);
    if (department.empty()) {
        return std::unexpected(EmployeeError::invalid_department);
    }
    if (fields.salary.is_negative()) {
        return std::unexpected(EmployeeError::invalid_salary);
    }
    auto email = normalize_email(fields.email);
    if (!email) {
        return std::unexpected(email.error());
    }
    return Employee(fields.id, std::string(name), std::move(*email), std::string(department), fields.salary,
                    normalize_skills(fields.skills));
}

std::expected<Employee, EmployeeError> Employee::from_csv_row(std::span<const std::string_view> row) {
    if (row.size() != 7) {
        return std::unexpected(EmployeeError::wrong_column_count);
    }

    std::int64_t id = 0;
    const char* first = row[0].data();
    const char* last = first + row[0].size();
    const auto [ptr, ec] = std::from_chars(first, last, id);
    if (ec != std::errc{} || ptr != last) {
        return std::unexpected(EmployeeError::invalid_id);
    }

    const auto currency = Currency::parse(row[5]);
    if (!currency) {
        return std::unexpected(EmployeeError::invalid_currency);
    }
    auto salary = Money::parse(row[4], *currency);
    if (!salary) {
        return std::unexpected(EmployeeError::invalid_salary);
    }

    std::vector<std::string> skills;
    std::string_view rest = row[6];
    while (true) {
        const auto semicolon = rest.find(';');
        skills.emplace_back(rest.substr(0, semicolon));
        if (semicolon == std::string_view::npos) {
            break;
        }
        rest.remove_prefix(semicolon + 1);
    }

    return create(EmployeeFields{
        .id = id,
        .name = std::string(row[1]),
        .email = std::string(row[2]),
        .department = std::string(row[3]),
        .salary = *salary,
        .skills = std::move(skills),
    });
}

std::int64_t Employee::id() const {
    return id_;
}

std::string_view Employee::name() const {
    return name_;
}

std::string_view Employee::email() const {
    return email_;
}

std::string_view Employee::department() const {
    return department_;
}

const Money& Employee::salary() const {
    return salary_;
}

const std::vector<std::string>& Employee::skills() const {
    return skills_;
}

bool operator==(const Employee& lhs, const Employee& rhs) {
    return lhs.id_ == rhs.id_ && lhs.name_ == rhs.name_ && lhs.email_ == rhs.email_ &&
           lhs.department_ == rhs.department_ && lhs.salary_ == rhs.salary_ && lhs.skills_ == rhs.skills_;
}

}  // namespace cppref::classes
