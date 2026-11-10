#include "repositories.hpp"

#include <chrono>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace cppref::database_sqlite {

namespace {

Timestamp to_timestamp(std::int64_t epoch_seconds) {
    return Timestamp{std::chrono::seconds{epoch_seconds}};
}

std::int64_t to_epoch(Timestamp ts) { return ts.time_since_epoch().count(); }

Customer customer_from_row(const Statement& stmt) {
    auto [id, email, name, created] = stmt.row<std::int64_t, std::string, std::string, std::int64_t>();
    return Customer{id, std::move(email), std::move(name), to_timestamp(created)};
}

Product product_from_row(const Statement& stmt) {
    auto [sku, name, cents] = stmt.row<std::string, std::string, std::int64_t>();
    return Product{std::move(sku), std::move(name), Money{cents}};
}

Order order_from_row(const Statement& stmt) {
    auto [id, customer_id, placed, status] = stmt.row<std::int64_t, std::int64_t, std::int64_t, std::string>();
    return Order{id, customer_id, to_timestamp(placed), std::move(status)};
}

constexpr const char* kUpsertProduct =
    "INSERT INTO products(sku, name, unit_price_cents) VALUES (?, ?, ?) "
    "ON CONFLICT(sku) DO UPDATE SET name = excluded.name, "
    "unit_price_cents = excluded.unit_price_cents";

void require_valid_price(const Product& product) {
    if (product.unit_price.cents < 0) {
        throw std::invalid_argument("unit price must not be negative: " + product.sku);
    }
}

std::string join(const std::vector<std::string>& items) {
    std::string out;
    for (const std::string& item : items) {
        if (!out.empty()) {
            out += ", ";
        }
        out += item;
    }
    return out;
}

}  // namespace

// ---- UnknownProductError --------------------------------------------------------

UnknownProductError::UnknownProductError(std::vector<std::string> skus)
    : std::runtime_error("unknown product SKU(s): " + join(skus)), skus_(std::move(skus)) {}

const std::vector<std::string>& UnknownProductError::skus() const noexcept { return skus_; }

// ---- CustomerRepository ---------------------------------------------------------

CustomerRepository::CustomerRepository(Database& db) noexcept : db_(&db) {}

Customer CustomerRepository::add(std::string_view email, std::string_view name, Timestamp created_at) {
    Statement stmt = db_->prepare("INSERT INTO customers(email, name, created_at) VALUES (?, ?, ?)");
    stmt.bind_all(email, name, to_epoch(created_at));
    stmt.step();
    return Customer{db_->last_insert_rowid(), std::string(email), std::string(name), created_at};
}

std::optional<Customer> CustomerRepository::get(std::int64_t id) {
    Statement stmt = db_->prepare("SELECT id, email, name, created_at FROM customers WHERE id = ?");
    stmt.bind_all(id);
    if (!stmt.step()) {
        return std::nullopt;
    }
    return customer_from_row(stmt);
}

std::optional<Customer> CustomerRepository::get_by_email(std::string_view email) {
    // The email column is declared COLLATE NOCASE, so plain equality is case-insensitive.
    Statement stmt = db_->prepare("SELECT id, email, name, created_at FROM customers WHERE email = ?");
    stmt.bind_all(email);
    if (!stmt.step()) {
        return std::nullopt;
    }
    return customer_from_row(stmt);
}

std::vector<Customer> CustomerRepository::search_by_name(std::string_view term) {
    if (term.empty()) {
        throw std::invalid_argument("search term must not be empty");
    }
    // Escape the LIKE wildcards with '!' so the term matches literally.
    std::string pattern = "%";
    for (char c : term) {
        if (c == '!' || c == '%' || c == '_') {
            pattern.push_back('!');
        }
        pattern.push_back(c);
    }
    pattern.push_back('%');

    Statement stmt = db_->prepare(
        "SELECT id, email, name, created_at FROM customers WHERE name LIKE ? ESCAPE '!' ORDER BY id");
    stmt.bind_all(pattern);
    std::vector<Customer> result;
    while (stmt.step()) {
        result.push_back(customer_from_row(stmt));
    }
    return result;
}

// ---- ProductRepository ----------------------------------------------------------

ProductRepository::ProductRepository(Database& db) noexcept : db_(&db) {}

void ProductRepository::upsert(const Product& product) {
    require_valid_price(product);
    Statement stmt = db_->prepare(kUpsertProduct);
    stmt.bind_all(product.sku, product.name, product.unit_price.cents);
    stmt.step();
}

void ProductRepository::upsert_many(std::span<const Product> products) {
    for (const Product& product : products) {
        require_valid_price(product);
    }
    Transaction tx(*db_);
    Statement stmt = db_->prepare(kUpsertProduct);
    for (const Product& product : products) {
        stmt.bind_all(product.sku, product.name, product.unit_price.cents);
        stmt.step();
        stmt.reset();
        stmt.clear_bindings();
    }
    tx.commit();
}

std::optional<Product> ProductRepository::get(std::string_view sku) {
    Statement stmt = db_->prepare("SELECT sku, name, unit_price_cents FROM products WHERE sku = ?");
    stmt.bind_all(sku);
    if (!stmt.step()) {
        return std::nullopt;
    }
    return product_from_row(stmt);
}

// ---- OrderRepository ------------------------------------------------------------

OrderRepository::OrderRepository(Database& db) noexcept : db_(&db) {}

std::int64_t OrderRepository::place(std::int64_t customer_id, Timestamp placed_at,
                                    std::span<const OrderLineInput> lines) {
    if (lines.empty()) {
        throw std::invalid_argument("an order needs at least one line");
    }
    for (const OrderLineInput& line : lines) {
        if (line.quantity < 1) {
            throw std::invalid_argument("line quantity must be at least 1: " + line.sku);
        }
    }

    // Rolls back on any throw below; nothing is written until every SKU has resolved.
    Transaction tx(*db_);

    std::vector<std::int64_t> prices(lines.size());
    std::set<std::string> missing;
    {
        Statement price_of = db_->prepare("SELECT unit_price_cents FROM products WHERE sku = ?");
        for (std::size_t i = 0; i < lines.size(); ++i) {
            price_of.bind_all(lines[i].sku);
            if (price_of.step()) {
                prices[i] = price_of.column<std::int64_t>(0);
            } else {
                missing.insert(lines[i].sku);
            }
            price_of.reset();
            price_of.clear_bindings();
        }
    }
    if (!missing.empty()) {
        throw UnknownProductError(std::vector<std::string>(missing.begin(), missing.end()));
    }

    Statement header = db_->prepare(
        "INSERT INTO orders(customer_id, placed_at, status) VALUES (?, ?, 'pending')");
    header.bind_all(customer_id, to_epoch(placed_at));
    header.step();  // a missing customer fails the foreign key check here
    const std::int64_t order_id = db_->last_insert_rowid();

    Statement insert_line = db_->prepare(
        "INSERT INTO order_lines(order_id, line_no, sku, quantity, unit_price_cents) "
        "VALUES (?, ?, ?, ?, ?)");
    for (std::size_t i = 0; i < lines.size(); ++i) {
        insert_line.bind_all(order_id, static_cast<std::int64_t>(i + 1), lines[i].sku,
                             lines[i].quantity, prices[i]);
        insert_line.step();
        insert_line.reset();
        insert_line.clear_bindings();
    }

    tx.commit();
    return order_id;
}

std::optional<Order> OrderRepository::get(std::int64_t order_id) {
    Statement stmt = db_->prepare("SELECT id, customer_id, placed_at, status FROM orders WHERE id = ?");
    stmt.bind_all(order_id);
    if (!stmt.step()) {
        return std::nullopt;
    }
    return order_from_row(stmt);
}

void OrderRepository::set_status(std::int64_t order_id, std::string_view status) {
    if (status != "pending" && status != "paid" && status != "shipped" && status != "cancelled") {
        throw std::invalid_argument("unknown order status: " + std::string(status));
    }
    Statement stmt = db_->prepare("UPDATE orders SET status = ? WHERE id = ?");
    stmt.bind_all(status, order_id);
    stmt.step();
    if (db_->changes() == 0) {
        throw std::out_of_range("no order with id " + std::to_string(order_id));
    }
}

std::vector<OrderLine> OrderRepository::lines(std::int64_t order_id) {
    Statement stmt = db_->prepare(
        "SELECT order_id, line_no, sku, quantity, unit_price_cents FROM order_lines "
        "WHERE order_id = ? ORDER BY line_no");
    stmt.bind_all(order_id);
    std::vector<OrderLine> result;
    while (stmt.step()) {
        auto [oid, line_no, sku, quantity, cents] =
            stmt.row<std::int64_t, std::int64_t, std::string, std::int64_t, std::int64_t>();
        result.push_back(OrderLine{oid, line_no, std::move(sku), quantity, Money{cents}});
    }
    return result;
}

}  // namespace cppref::database_sqlite
