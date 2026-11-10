#include "validation.hpp"

#include <algorithm>
#include <cstdint>
#include <format>
#include <regex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace cppref::functional {

namespace {

const Value* find_member(const Object& object, std::string_view key) {
    for (const auto& [name, value] : object) {
        if (name == key) {
            return &value;
        }
    }
    return nullptr;
}

}  // namespace

ValueKind kind_of(const Value& value) {
    if (std::holds_alternative<std::monostate>(value.data)) {
        return ValueKind::null;
    }
    if (std::holds_alternative<bool>(value.data)) {
        return ValueKind::boolean;
    }
    if (std::holds_alternative<std::int64_t>(value.data)) {
        return ValueKind::integer;
    }
    if (std::holds_alternative<double>(value.data)) {
        return ValueKind::real;
    }
    if (std::holds_alternative<std::string>(value.data)) {
        return ValueKind::string;
    }
    if (std::holds_alternative<Array>(value.data)) {
        return ValueKind::array;
    }
    return ValueKind::object;
}

std::string_view kind_name(ValueKind kind) {
    switch (kind) {
        case ValueKind::null:
            return "None";
        case ValueKind::boolean:
            return "bool";
        case ValueKind::integer:
            return "int";
        case ValueKind::real:
            return "float";
        case ValueKind::string:
            return "str";
        case ValueKind::array:
            return "list";
        case ValueKind::object:
            return "Mapping";
    }
    return "unknown";
}

Validator is_type(ValueKind expected) {
    return [expected](const Value& value) -> Errors {
        const ValueKind actual = kind_of(value);
        if (actual == expected) {
            return {};
        }
        return {std::format("expected {}, got {}", kind_name(expected), kind_name(actual))};
    };
}

Validator min_length(std::size_t length) {
    return [length](const Value& value) -> Errors {
        const auto* text = std::get_if<std::string>(&value.data);
        if (text == nullptr || text->size() >= length) {
            return {};
        }
        return {std::format("length must be at least {}", length)};
    };
}

Validator matches(std::string pattern) {
    std::regex re{pattern};
    return [re, pattern](const Value& value) -> Errors {
        const auto* text = std::get_if<std::string>(&value.data);
        if (text == nullptr || std::regex_match(*text, re)) {
            return {};
        }
        return {std::format("must match pattern '{}'", pattern)};
    };
}

Validator in_range(double low, double high) {
    return [low, high](const Value& value) -> Errors {
        double number = 0.0;
        if (const auto* integer = std::get_if<std::int64_t>(&value.data)) {
            number = static_cast<double>(*integer);
        } else if (const auto* real = std::get_if<double>(&value.data)) {
            number = *real;
        } else {
            return {};
        }
        if (number < low || number > high) {
            return {std::format("must be between {} and {}", low, high)};
        }
        return {};
    };
}

Validator all_of(std::initializer_list<Validator> validators) {
    std::vector<Validator> checks(validators);
    return [checks](const Value& value) -> Errors {
        Errors out;
        for (const Validator& check : checks) {
            Errors errors = check(value);
            out.insert(out.end(), errors.begin(), errors.end());
        }
        return out;
    };
}

Validator any_of(std::initializer_list<Validator> validators) {
    if (validators.size() == 0) {
        throw std::invalid_argument("any_of requires at least one validator");
    }
    std::vector<Validator> checks(validators);
    return [checks](const Value& value) -> Errors {
        Errors all;
        for (const Validator& check : checks) {
            Errors errors = check(value);
            if (errors.empty()) {
                return {};
            }
            all.insert(all.end(), errors.begin(), errors.end());
        }
        return all;
    };
}

Validator field(std::string name, Validator validator) {
    return [name = std::move(name), validator = std::move(validator)](const Value& value) -> Errors {
        const auto* object = std::get_if<Object>(&value.data);
        if (object == nullptr) {
            return {};
        }
        const Value* member = find_member(*object, name);
        if (member == nullptr) {
            return {name + ": missing"};
        }
        Errors errors = validator(*member);
        for (std::string& error : errors) {
            error = name + ": " + error;
        }
        return errors;
    };
}

Validator optional(Validator validator) {
    return [validator = std::move(validator)](const Value& value) -> Errors {
        if (std::holds_alternative<std::monostate>(value.data)) {
            return {};
        }
        return validator(value);
    };
}

Validator each(Validator validator) {
    return [validator = std::move(validator)](const Value& value) -> Errors {
        const auto* array = std::get_if<Array>(&value.data);
        if (array == nullptr) {
            return {};
        }
        Errors out;
        for (std::size_t i = 0; i < array->size(); ++i) {
            for (const std::string& error : validator((*array)[i])) {
                out.push_back(std::format("[{}] {}", i, error));
            }
        }
        return out;
    };
}

Errors validate_signup(const Value& payload) {
    const auto* object = std::get_if<Object>(&payload.data);
    if (object == nullptr) {
        return {std::format("expected Mapping, got {}", kind_name(kind_of(payload)))};
    }

    static const Validator username =
        all_of({is_type(ValueKind::string), min_length(3), matches("[a-z0-9_]+")});
    static const Validator age = all_of({is_type(ValueKind::integer), in_range(13, 120)});
    static const Validator email =
        all_of({is_type(ValueKind::string), matches(R"([^@\s]+@[^@\s]+\.[^@\s]+)")});
    static const Validator tags = optional(all_of({
        is_type(ValueKind::array),
        each(all_of({is_type(ValueKind::string), min_length(1)})),
    }));

    Errors out;
    auto append = [&out](Errors errors) { out.insert(out.end(), errors.begin(), errors.end()); };
    append(field("username", username)(payload));
    append(field("age", age)(payload));
    append(field("email", email)(payload));
    if (find_member(*object, "tags") != nullptr) {
        append(field("tags", tags)(payload));
    }
    return out;
}

}  // namespace cppref::functional
