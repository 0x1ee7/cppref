#pragma once

#include "model.hpp"
#include "sqlite_wrapper.hpp"

#include <cstddef>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace cppref::database_sqlite {

// Thrown by OrderRepository::place when lines reference SKUs absent from the catalogue.
class UnknownProductError : public std::runtime_error {
public:
    // skus: the missing SKUs; sorted and de-duplicated by the caller.
    explicit UnknownProductError(std::vector<std::string> skus);

    // Sorted, de-duplicated missing SKUs.
    const std::vector<std::string>& skus() const noexcept;

    // Engineer-owned: store skus as a private member.
};

// Non-owning repositories: the Database must outlive them.
class CustomerRepository {
public:
    explicit CustomerRepository(Database& db) noexcept;

    // Inserts and returns the stored row with its generated id.
    // Duplicate email (any letter case) throws DbError(SQLITE_CONSTRAINT_UNIQUE).
    Customer add(std::string_view email, std::string_view name, Timestamp created_at);

    std::optional<Customer> get(std::int64_t id);

    // Case-insensitive email lookup.
    std::optional<Customer> get_by_email(std::string_view email);

    // Literal, case-insensitive substring match on name, ordered by id.
    // Empty term throws std::invalid_argument. '%' and '_' match literally.
    std::vector<Customer> search_by_name(std::string_view term);

    // Engineer-owned: Database& member.
};

class ProductRepository {
public:
    explicit ProductRepository(Database& db) noexcept;

    // Inserts or replaces (name and price) by sku. Negative price: std::invalid_argument.
    void upsert(const Product& product);

    // Validates every row first (nothing is written if any row is invalid), then upserts
    // all rows in one transaction, reusing one prepared statement.
    void upsert_many(std::span<const Product> products);

    std::optional<Product> get(std::string_view sku);

    // Engineer-owned: Database& member.
};

class OrderRepository {
public:
    explicit OrderRepository(Database& db) noexcept;

    // Places an order atomically and returns its id.
    //  - lines empty or any quantity < 1: std::invalid_argument
    //  - unknown SKUs: UnknownProductError listing every missing SKU
    //  - unknown customer: DbError(SQLITE_CONSTRAINT_FOREIGNKEY)
    // Unit prices are snapshotted from the catalogue. line_no starts at 1 in input order.
    // Any failure leaves no order header and no lines.
    std::int64_t place(std::int64_t customer_id, Timestamp placed_at,
                       std::span<const OrderLineInput> lines);

    std::optional<Order> get(std::int64_t order_id);

    // status must be pending, paid, shipped or cancelled (std::invalid_argument).
    // Unknown order id: std::out_of_range.
    void set_status(std::int64_t order_id, std::string_view status);

    // Lines ordered by line_no.
    std::vector<OrderLine> lines(std::int64_t order_id);

    // Engineer-owned: Database& member.
};

// Streams rows into the catalogue in batches of batch_size; each batch is one transaction
// (upsert semantics). Returns the number of rows processed.
//  - batch_size == 0: std::invalid_argument
//  - a bad row (for example a negative price) throws std::invalid_argument. Batches already
//    committed stay; the batch holding the bad row rolls back in full; later rows are not read.
// Accepts a single-pass input range; memory must not grow with the input size.
template <std::ranges::input_range R>
    requires std::convertible_to<std::ranges::range_reference_t<R>, Product>
std::size_t import_products(Database& db, R&& rows, std::size_t batch_size = 500) {
    (void)db;
    (void)rows;
    (void)batch_size;
    throw std::logic_error("not implemented: import_products");
}

}  // namespace cppref::database_sqlite
