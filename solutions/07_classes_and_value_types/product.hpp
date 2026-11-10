// Part 3: Product — validated catalogue entry with a class-wide creation counter.
#pragma once

#include <cstddef>
#include <expected>
#include <string>
#include <string_view>

#include "money.hpp"

namespace cppref::classes {

enum class ProductError {
    invalid_sku,       ///< not 4-16 characters from A-Z, 0-9, '-'
    invalid_name,      ///< blank after trimming, or longer than 100 bytes
    invalid_quantity,  ///< negative quantity, or restock count < 1
    overflow,          ///< quantity would exceed INT_MAX
};

/// Catalogue entry. Invariants hold for every live object.
class Product {
public:
    [[nodiscard]] static std::expected<Product, ProductError> create(std::string sku, std::string name,
                                                                     Money price, int quantity = 0);

    /// Number of successful create() calls so far. Copies are not counted.
    [[nodiscard]] static std::size_t total_created();

    [[nodiscard]] std::string_view sku() const;
    [[nodiscard]] std::string_view name() const;
    [[nodiscard]] const Money& price() const;
    [[nodiscard]] int quantity() const;
    [[nodiscard]] bool in_stock() const;

    [[nodiscard]] std::expected<void, ProductError> set_name(std::string_view name);
    void set_price(Money price);
    [[nodiscard]] std::expected<void, ProductError> set_quantity(int quantity);
    [[nodiscard]] std::expected<void, ProductError> restock(int count);

private:
    Product(std::string sku, std::string name, Money price, int quantity)
        : sku_(std::move(sku)), name_(std::move(name)), price_(std::move(price)), quantity_(quantity) {}

    std::string sku_;
    std::string name_;
    Money price_;
    int quantity_;
};

}  // namespace cppref::classes
