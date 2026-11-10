#include "versioned_api.hpp"

#include <stdexcept>

namespace orders::v2 {

std::string describe(const Order&) {
    throw std::logic_error("not implemented: orders::v2::describe");
}

std::string to_json(const Order&) {
    throw std::logic_error("not implemented: orders::v2::to_json");
}

}  // namespace orders::v2

namespace orders::v1 {

std::string describe(const Order&) {
    throw std::logic_error("not implemented: orders::v1::describe");
}

std::string to_json(const Order&) {
    throw std::logic_error("not implemented: orders::v1::to_json");
}

}  // namespace orders::v1

namespace json {

std::string quote(std::string_view) {
    throw std::logic_error("not implemented: json::quote");
}

std::string dump(std::span<const std::string>) {
    throw std::logic_error("not implemented: json::dump");
}

}  // namespace json
