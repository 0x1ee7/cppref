#include "reports.hpp"

#include <stdexcept>

namespace cppref::database_sqlite {

std::vector<ProductSales> units_sold_by_product(Database& db) {
    (void)db;
    throw std::logic_error("not implemented: units_sold_by_product");
}

std::vector<CustomerRevenue> revenue_by_customer(Database& db) {
    (void)db;
    throw std::logic_error("not implemented: revenue_by_customer");
}

std::vector<RankedCustomer> rank_customers_by_orders(Database& db) {
    (void)db;
    throw std::logic_error("not implemented: rank_customers_by_orders");
}

std::vector<RunningUnits> customer_running_units(Database& db, std::int64_t customer_id) {
    (void)db;
    (void)customer_id;
    throw std::logic_error("not implemented: customer_running_units");
}

}  // namespace cppref::database_sqlite
