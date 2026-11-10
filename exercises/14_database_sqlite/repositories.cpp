#include "repositories.hpp"

#include <stdexcept>

namespace cppref::database_sqlite {

UnknownProductError::UnknownProductError(std::vector<std::string> skus)
    : std::runtime_error("unknown product sku") {
    (void)skus;
    throw std::logic_error("not implemented: UnknownProductError::UnknownProductError");
}

// noexcept accessor returns a placeholder: throwing here would call std::terminate.
const std::vector<std::string>& UnknownProductError::skus() const noexcept {
    static const std::vector<std::string> none;
    return none;
}

CustomerRepository::CustomerRepository(Database& db) noexcept {
    (void)db;
}

Customer CustomerRepository::add(std::string_view email, std::string_view name, Timestamp created_at) {
    (void)email;
    (void)name;
    (void)created_at;
    throw std::logic_error("not implemented: CustomerRepository::add");
}

std::optional<Customer> CustomerRepository::get(std::int64_t id) {
    (void)id;
    throw std::logic_error("not implemented: CustomerRepository::get");
}

std::optional<Customer> CustomerRepository::get_by_email(std::string_view email) {
    (void)email;
    throw std::logic_error("not implemented: CustomerRepository::get_by_email");
}

std::vector<Customer> CustomerRepository::search_by_name(std::string_view term) {
    (void)term;
    throw std::logic_error("not implemented: CustomerRepository::search_by_name");
}

ProductRepository::ProductRepository(Database& db) noexcept {
    (void)db;
}

void ProductRepository::upsert(const Product& product) {
    (void)product;
    throw std::logic_error("not implemented: ProductRepository::upsert");
}

void ProductRepository::upsert_many(std::span<const Product> products) {
    (void)products;
    throw std::logic_error("not implemented: ProductRepository::upsert_many");
}

std::optional<Product> ProductRepository::get(std::string_view sku) {
    (void)sku;
    throw std::logic_error("not implemented: ProductRepository::get");
}

OrderRepository::OrderRepository(Database& db) noexcept {
    (void)db;
}

std::int64_t OrderRepository::place(std::int64_t customer_id, Timestamp placed_at,
                                    std::span<const OrderLineInput> lines) {
    (void)customer_id;
    (void)placed_at;
    (void)lines;
    throw std::logic_error("not implemented: OrderRepository::place");
}

std::optional<Order> OrderRepository::get(std::int64_t order_id) {
    (void)order_id;
    throw std::logic_error("not implemented: OrderRepository::get");
}

void OrderRepository::set_status(std::int64_t order_id, std::string_view status) {
    (void)order_id;
    (void)status;
    throw std::logic_error("not implemented: OrderRepository::set_status");
}

std::vector<OrderLine> OrderRepository::lines(std::int64_t order_id) {
    (void)order_id;
    throw std::logic_error("not implemented: OrderRepository::lines");
}

}  // namespace cppref::database_sqlite
