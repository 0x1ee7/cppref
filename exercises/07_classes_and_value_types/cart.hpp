// Part 5: ShoppingCart — a container with begin/end, keyed lookup and value semantics.
#pragma once

#include <cstddef>
#include <expected>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "money.hpp"
#include "product.hpp"

namespace cppref::classes {

enum class CartError {
    invalid_quantity,      ///< quantity < 1
    currency_mismatch,     ///< product price currency differs from the cart currency
    not_found,             ///< remove(): sku is not in the cart
    insufficient_quantity, ///< remove(): more units than the line holds
    overflow,              ///< total units would exceed INT_MAX
};

/// One cart line as seen by iteration. Lines are kept in insertion order.
struct CartLine {
    std::string sku;
    int quantity;
    Money unit_price;  ///< price of the most recent add() for this sku
};

class ShoppingCart {
public:
    using const_iterator = std::vector<CartLine>::const_iterator;

    explicit ShoppingCart(Currency currency);

    [[nodiscard]] Currency currency() const;

    /// Adding an existing sku accumulates quantity and adopts the product's current price.
    [[nodiscard]] std::expected<void, CartError> add(const Product& product, int quantity = 1);

    /// Removing the last unit deletes the line.
    [[nodiscard]] std::expected<void, CartError> remove(std::string_view sku, int quantity = 1);

    /// Sum of unit_price * quantity over all lines; zero in the cart currency when empty.
    [[nodiscard]] Money total() const;

    /// Total units across all lines.
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] bool empty() const;

    [[nodiscard]] bool contains(std::string_view sku) const;
    [[nodiscard]] bool contains(const Product& product) const;

    /// Quantity for sku. Throws std::out_of_range when the sku is absent.
    [[nodiscard]] int operator[](std::string_view sku) const;

    [[nodiscard]] const_iterator begin() const;
    [[nodiscard]] const_iterator end() const;

    /// Writes e.g. "ShoppingCart(currency='USD', items=3)".
    friend std::ostream& operator<<(std::ostream& out, const ShoppingCart& cart);

    // engineer: private data members (Currency currency_; std::vector<CartLine> lines_;)
};

}  // namespace cppref::classes
