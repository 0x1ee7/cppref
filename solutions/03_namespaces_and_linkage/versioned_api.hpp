#pragma once

#include <span>
#include <string>
#include <string_view>

// Part 1 — inline namespaces, ADL and a tiny JSON encoder.
namespace orders {

// Current API. `orders::Order` (unqualified) names v2 because v2 is an inline namespace.
inline namespace v2 {

struct Order {
    std::string id;
    int quantity = 0;
    std::string currency;  // added in v2
};

// "<id> x<quantity> <currency>", e.g. "ord-1 x3 EUR".
std::string describe(const Order& order);

// {"id":"...","quantity":N,"currency":"..."} with JSON-escaped strings.
// Found by ADL from `to_json(order)`.
std::string to_json(const Order& order);

}  // namespace v2

// Legacy API, still shipped. Not inline: callers must write orders::v1::.
namespace v1 {

struct Order {
    std::string id;
    int quantity = 0;
};

// "<id> x<quantity>", e.g. "ord-2 x5".
std::string describe(const Order& order);

// {"id":"...","quantity":N}
std::string to_json(const Order& order);

}  // namespace v1

}  // namespace orders

namespace json {

// Returns s as a JSON string literal including the surrounding quotes.
// Escapes '"' and '\\' with a backslash, \n \t \r \b \f with short escapes, and any
// other control character (< 0x20) as \u00XX (lower-case hex).
std::string quote(std::string_view s);

// Joins already-encoded JSON values into an array: [a,b,c]. An empty span gives "[]".
// Does not quote or validate its inputs.
std::string dump(std::span<const std::string> encoded_values);

}  // namespace json
