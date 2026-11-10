#include "order_queries.hpp"

#include <stdexcept>

namespace cppref::database_sqlite {

std::string OrderCursor::encode() const {
    throw std::logic_error("not implemented: OrderCursor::encode");
}

OrderCursor OrderCursor::decode(std::string_view token) {
    (void)token;
    throw std::logic_error("not implemented: OrderCursor::decode");
}

OrderPage page_orders_keyset(Database& db, std::optional<OrderCursor> after, std::size_t limit) {
    (void)db;
    (void)after;
    (void)limit;
    throw std::logic_error("not implemented: page_orders_keyset");
}

std::vector<Order> page_orders_offset(Database& db, std::int64_t offset, std::int64_t limit) {
    (void)db;
    (void)offset;
    (void)limit;
    throw std::logic_error("not implemented: page_orders_offset");
}

std::vector<OrderWithItems> load_orders_with_items(Database& db, std::int64_t customer_id) {
    (void)db;
    (void)customer_id;
    throw std::logic_error("not implemented: load_orders_with_items");
}

}  // namespace cppref::database_sqlite
