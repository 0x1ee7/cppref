#pragma once

#include "model.hpp"
#include "sqlite_wrapper.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::database_sqlite {

// Opaque keyset cursor: the position after (placed_at, order_id) in ascending
// (placed_at, id) order.
struct OrderCursor {
    Timestamp placed_at{};
    std::int64_t order_id = 0;

    // URL-safe base64 without padding of "<epoch seconds>:<order_id>".
    std::string encode() const;

    // Throws std::invalid_argument on anything malformed: empty input, characters outside
    // the base64url alphabet, bad length, missing ':', non-integer parts, trailing junk.
    static OrderCursor decode(std::string_view token);

    bool operator==(const OrderCursor&) const = default;
};

struct OrderPage {
    std::vector<Order> items;
    std::optional<OrderCursor> next_cursor;
    bool operator==(const OrderPage&) const = default;
};

// Keyset page in (placed_at, id) ascending order, starting after `after` (or at the
// beginning). next_cursor is set when items.size() == limit, even if nothing follows.
// limit == 0: std::invalid_argument. Pages do not overlap when older rows are inserted.
OrderPage page_orders_keyset(Database& db, std::optional<OrderCursor> after, std::size_t limit);

// Positional alternative with the same ordering. offset < 0 or limit < 1:
// std::invalid_argument. Expected to shift when rows are inserted before the offset.
std::vector<Order> page_orders_offset(Database& db, std::int64_t offset, std::int64_t limit);

struct OrderWithItems {
    Order order;
    std::vector<OrderLine> lines;
    bool operator==(const OrderWithItems&) const = default;
};

// The customer's orders (cancelled included) in (placed_at, id) order, each with its lines
// ordered by line_no. A customer with no orders gets an empty vector. Uses a bounded number
// of statements regardless of the order count.
std::vector<OrderWithItems> load_orders_with_items(Database& db, std::int64_t customer_id);

}  // namespace cppref::database_sqlite
