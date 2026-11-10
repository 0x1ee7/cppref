#include "reports.hpp"

#include <chrono>
#include <cstdint>
#include <string>
#include <utility>

namespace cppref::database_sqlite {

std::vector<ProductSales> units_sold_by_product(Database& db) {
    Statement stmt = db.prepare(
        "SELECT ol.sku, SUM(ol.quantity), COUNT(DISTINCT ol.order_id) "
        "FROM order_lines ol JOIN orders o ON o.id = ol.order_id "
        "WHERE o.status <> 'cancelled' "
        "GROUP BY ol.sku "
        "ORDER BY SUM(ol.quantity) DESC, ol.sku ASC");
    std::vector<ProductSales> result;
    while (stmt.step()) {
        auto [sku, units, orders] = stmt.row<std::string, std::int64_t, std::int64_t>();
        result.push_back(ProductSales{std::move(sku), units, orders});
    }
    return result;
}

std::vector<CustomerRevenue> revenue_by_customer(Database& db) {
    Statement stmt = db.prepare(
        "SELECT c.id, c.name, COUNT(DISTINCT o.id), SUM(ol.quantity * ol.unit_price_cents) "
        "FROM customers c "
        "JOIN orders o ON o.customer_id = c.id "
        "JOIN order_lines ol ON ol.order_id = o.id "
        "WHERE o.status <> 'cancelled' "
        "GROUP BY c.id, c.name "
        "ORDER BY c.id");
    std::vector<CustomerRevenue> result;
    while (stmt.step()) {
        auto [id, name, orders, cents] =
            stmt.row<std::int64_t, std::string, std::int64_t, std::int64_t>();
        result.push_back(CustomerRevenue{id, std::move(name), orders, Money{cents}});
    }
    return result;
}

std::vector<RankedCustomer> rank_customers_by_orders(Database& db) {
    Statement stmt = db.prepare(
        "WITH counts AS ("
        "  SELECT c.id, c.name, ("
        "    SELECT COUNT(*) FROM orders o "
        "    WHERE o.customer_id = c.id AND o.status <> 'cancelled') AS n "
        "  FROM customers c) "
        "SELECT id, name, n, RANK() OVER (ORDER BY n DESC) AS rnk "
        "FROM counts ORDER BY rnk, name");
    std::vector<RankedCustomer> result;
    while (stmt.step()) {
        auto [id, name, orders, rank] =
            stmt.row<std::int64_t, std::string, std::int64_t, std::int64_t>();
        result.push_back(RankedCustomer{id, std::move(name), orders, rank});
    }
    return result;
}

std::vector<RunningUnits> customer_running_units(Database& db, std::int64_t customer_id) {
    Statement stmt = db.prepare(
        "WITH per_order AS ("
        "  SELECT o.id, o.placed_at, SUM(ol.quantity) AS units "
        "  FROM orders o JOIN order_lines ol ON ol.order_id = o.id "
        "  WHERE o.customer_id = ? AND o.status <> 'cancelled' "
        "  GROUP BY o.id, o.placed_at) "
        "SELECT id, placed_at, units, SUM(units) OVER (ORDER BY placed_at, id) "
        "FROM per_order ORDER BY placed_at, id");
    stmt.bind_all(customer_id);
    std::vector<RunningUnits> result;
    while (stmt.step()) {
        auto [id, placed, units, running] =
            stmt.row<std::int64_t, std::int64_t, std::int64_t, std::int64_t>();
        result.push_back(RunningUnits{id, Timestamp{std::chrono::seconds{placed}}, units, running});
    }
    return result;
}

}  // namespace cppref::database_sqlite
