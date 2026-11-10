#include "versioned_api.hpp"

#include <cstddef>
#include <format>
#include <string>
#include <string_view>

namespace orders {

namespace v2 {

std::string describe(const Order& order) {
    return std::format("{} x{} {}", order.id, order.quantity, order.currency);
}

std::string to_json(const Order& order) {
    return std::format(R"({{"id":{},"quantity":{},"currency":{}}})", json::quote(order.id),
                       order.quantity, json::quote(order.currency));
}

}  // namespace v2

namespace v1 {

std::string describe(const Order& order) {
    return std::format("{} x{}", order.id, order.quantity);
}

std::string to_json(const Order& order) {
    return std::format(R"({{"id":{},"quantity":{}}})", json::quote(order.id), order.quantity);
}

}  // namespace v1

}  // namespace orders

namespace json {

std::string quote(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (const unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\t': out += "\\t"; break;
            case '\r': out += "\\r"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    out += std::format("\\u{:04x}", static_cast<unsigned>(c));
                } else {
                    out.push_back(static_cast<char>(c));
                }
        }
    }
    out.push_back('"');
    return out;
}

std::string dump(std::span<const std::string> encoded_values) {
    std::string out = "[";
    for (std::size_t i = 0; i < encoded_values.size(); ++i) {
        if (i != 0) out.push_back(',');
        out += encoded_values[i];
    }
    out.push_back(']');
    return out;
}

}  // namespace json
