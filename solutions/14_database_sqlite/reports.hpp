#pragma once

#include "model.hpp"
#include "sqlite_wrapper.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace cppref::database_sqlite {

// Every report excludes orders with status 'cancelled'.

struct ProductSales {
    std::string sku;
    std::int64_t units_sold = 0;
    std::int64_t order_count = 0;  // distinct non-cancelled orders containing the SKU
    bool operator==(const ProductSales&) const = default;
};

struct CustomerRevenue {
    std::int64_t customer_id = 0;
    std::string name;
    std::int64_t order_count = 0;
    Money total{};  // exact sum of quantity * unit_price over non-cancelled lines
    bool operator==(const CustomerRevenue&) const = default;
};

struct RankedCustomer {
    std::int64_t customer_id = 0;
    std::string name;
    std::int64_t order_count = 0;
    std::int64_t rank = 0;
    bool operator==(const RankedCustomer&) const = default;
};

struct RunningUnits {
    std::int64_t order_id = 0;
    Timestamp placed_at{};
    std::int64_t units = 0;          // total units in this order
    std::int64_t running_units = 0;  // cumulative units across this customer's orders
    bool operator==(const RunningUnits&) const = default;
};

// Units descending, then sku ascending. Only SKUs with at least one non-cancelled order.
std::vector<ProductSales> units_sold_by_product(Database& db);

// Sorted by customer_id. Customers without non-cancelled orders are omitted.
std::vector<CustomerRevenue> revenue_by_customer(Database& db);

// Every customer, including those with zero orders, ranked by non-cancelled order count
// descending using SQL RANK() (ties share a rank; the next rank is skipped).
// Sorted by rank, then name.
std::vector<RankedCustomer> rank_customers_by_orders(Database& db);

// One row per non-cancelled order of the customer, ordered by (placed_at, order_id), with
// the order's units and a running total (SUM() OVER).
std::vector<RunningUnits> customer_running_units(Database& db, std::int64_t customer_id);

}  // namespace cppref::database_sqlite
