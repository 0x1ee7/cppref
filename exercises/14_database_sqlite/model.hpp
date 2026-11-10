#pragma once

#include <chrono>
#include <cstdint>
#include <string>

namespace cppref::database_sqlite {

// Money as integer minor units (cents). Never floating point.
struct Money {
    std::int64_t cents = 0;
    auto operator<=>(const Money&) const = default;
};

// UTC instant at whole-second precision. Stored as INTEGER epoch seconds.
using Timestamp = std::chrono::sys_seconds;

struct Customer {
    std::int64_t id = 0;
    std::string email;
    std::string name;
    Timestamp created_at{};
    bool operator==(const Customer&) const = default;
};

struct Product {
    std::string sku;
    std::string name;
    Money unit_price{};
    bool operator==(const Product&) const = default;
};

struct Order {
    std::int64_t id = 0;
    std::int64_t customer_id = 0;
    Timestamp placed_at{};
    std::string status;  // pending | paid | shipped | cancelled
    bool operator==(const Order&) const = default;
};

struct OrderLine {
    std::int64_t order_id = 0;
    std::int64_t line_no = 0;
    std::string sku;
    std::int64_t quantity = 0;
    Money unit_price{};  // snapshot taken when the order was placed
    bool operator==(const OrderLine&) const = default;
};

// Input to OrderRepository::place. The unit price is not supplied: it is read from the catalogue.
struct OrderLineInput {
    std::string sku;
    std::int64_t quantity = 0;
};

}  // namespace cppref::database_sqlite
