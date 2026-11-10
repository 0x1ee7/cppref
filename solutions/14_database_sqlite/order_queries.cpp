#include "order_queries.hpp"

#include <charconv>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace cppref::database_sqlite {

namespace {

constexpr std::string_view kAlphabet =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

Timestamp to_timestamp(std::int64_t epoch_seconds) {
    return Timestamp{std::chrono::seconds{epoch_seconds}};
}

std::int64_t to_epoch(Timestamp ts) { return ts.time_since_epoch().count(); }

int alphabet_value(char c) {
    const std::size_t pos = kAlphabet.find(c);
    if (pos == std::string_view::npos) {
        throw std::invalid_argument("cursor contains characters outside base64url");
    }
    return static_cast<int>(pos);
}

std::string base64url_encode(std::string_view bytes) {
    std::string out;
    std::uint32_t buffer = 0;
    int bits = 0;
    for (unsigned char byte : bytes) {
        buffer = (buffer << 8) | byte;
        bits += 8;
        while (bits >= 6) {
            bits -= 6;
            out.push_back(kAlphabet[(buffer >> bits) & 0x3F]);
        }
    }
    if (bits > 0) {
        out.push_back(kAlphabet[(buffer << (6 - bits)) & 0x3F]);
    }
    return out;
}

std::string base64url_decode(std::string_view token) {
    if (token.size() % 4 == 1) {
        throw std::invalid_argument("cursor has an impossible base64 length");
    }
    std::string out;
    std::uint32_t buffer = 0;
    int bits = 0;
    for (char c : token) {
        buffer = (buffer << 6) | static_cast<std::uint32_t>(alphabet_value(c));
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buffer >> bits) & 0xFF));
        }
    }
    return out;
}

std::int64_t parse_integer(std::string_view text) {
    std::int64_t value = 0;
    const char* first = text.data();
    const char* last = text.data() + text.size();
    const auto [ptr, ec] = std::from_chars(first, last, value);
    if (text.empty() || ec != std::errc{} || ptr != last) {
        throw std::invalid_argument("cursor contains a malformed integer");
    }
    return value;
}

Order order_from_row(const Statement& stmt) {
    auto [id, customer_id, placed, status] =
        stmt.row<std::int64_t, std::int64_t, std::int64_t, std::string>();
    return Order{id, customer_id, to_timestamp(placed), std::move(status)};
}

}  // namespace

// ---- OrderCursor ----------------------------------------------------------------

std::string OrderCursor::encode() const {
    return base64url_encode(std::to_string(to_epoch(placed_at)) + ":" + std::to_string(order_id));
}

OrderCursor OrderCursor::decode(std::string_view token) {
    if (token.empty()) {
        throw std::invalid_argument("cursor is empty");
    }
    const std::string bytes = base64url_decode(token);
    const std::size_t colon = bytes.find(':');
    if (colon == std::string::npos || bytes.find(':', colon + 1) != std::string::npos) {
        throw std::invalid_argument("cursor must contain exactly one ':'");
    }
    OrderCursor cursor;
    cursor.placed_at = to_timestamp(parse_integer(std::string_view(bytes).substr(0, colon)));
    cursor.order_id = parse_integer(std::string_view(bytes).substr(colon + 1));
    return cursor;
}

// ---- Pages ----------------------------------------------------------------------

OrderPage page_orders_keyset(Database& db, std::optional<OrderCursor> after, std::size_t limit) {
    if (limit == 0) {
        throw std::invalid_argument("page limit must be positive");
    }
    const auto sql_limit = static_cast<std::int64_t>(limit);
    Statement stmt = after.has_value()
        ? db.prepare(
              "SELECT id, customer_id, placed_at, status FROM orders "
              "WHERE placed_at > ? OR (placed_at = ? AND id > ?) "
              "ORDER BY placed_at, id LIMIT ?")
        : db.prepare(
              "SELECT id, customer_id, placed_at, status FROM orders "
              "ORDER BY placed_at, id LIMIT ?");
    if (after.has_value()) {
        const std::int64_t at = to_epoch(after->placed_at);
        stmt.bind_all(at, at, after->order_id, sql_limit);
    } else {
        stmt.bind_all(sql_limit);
    }

    OrderPage page;
    while (stmt.step()) {
        page.items.push_back(order_from_row(stmt));
    }
    if (page.items.size() == limit) {
        const Order& last = page.items.back();
        page.next_cursor = OrderCursor{last.placed_at, last.id};
    }
    return page;
}

std::vector<Order> page_orders_offset(Database& db, std::int64_t offset, std::int64_t limit) {
    if (offset < 0 || limit < 1) {
        throw std::invalid_argument("offset must be >= 0 and limit >= 1");
    }
    Statement stmt = db.prepare(
        "SELECT id, customer_id, placed_at, status FROM orders "
        "ORDER BY placed_at, id LIMIT ? OFFSET ?");
    stmt.bind_all(limit, offset);
    std::vector<Order> items;
    while (stmt.step()) {
        items.push_back(order_from_row(stmt));
    }
    return items;
}

// ---- Batch load -----------------------------------------------------------------

std::vector<OrderWithItems> load_orders_with_items(Database& db, std::int64_t customer_id) {
    Statement orders = db.prepare(
        "SELECT id, customer_id, placed_at, status FROM orders "
        "WHERE customer_id = ? ORDER BY placed_at, id");
    orders.bind_all(customer_id);

    std::vector<OrderWithItems> result;
    std::unordered_map<std::int64_t, std::size_t> index_of;
    while (orders.step()) {
        result.push_back(OrderWithItems{order_from_row(orders), {}});
        index_of.emplace(result.back().order.id, result.size() - 1);
    }

    // One statement for every line of the customer's orders, instead of one per order.
    Statement lines = db.prepare(
        "SELECT ol.order_id, ol.line_no, ol.sku, ol.quantity, ol.unit_price_cents "
        "FROM order_lines ol JOIN orders o ON o.id = ol.order_id "
        "WHERE o.customer_id = ? ORDER BY o.placed_at, o.id, ol.line_no");
    lines.bind_all(customer_id);
    while (lines.step()) {
        auto [order_id, line_no, sku, quantity, cents] =
            lines.row<std::int64_t, std::int64_t, std::string, std::int64_t, std::int64_t>();
        result[index_of.at(order_id)].lines.push_back(
            OrderLine{order_id, line_no, std::move(sku), quantity, Money{cents}});
    }
    return result;
}

}  // namespace cppref::database_sqlite
