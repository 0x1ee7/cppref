#pragma once

#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

// Part 3 · Redux-style cart reducer (engineer-owned).
//
// State is an immutable value. reduce_cart() never modifies its input; it
// returns a new CartState or an error. Actions are a closed set of alternatives
// in a std::variant, dispatched with std::visit + overloaded.
namespace cppref::functional {

struct LineItem {
    std::string sku;
    std::string name;
    std::int64_t unit_price_cents = 0;
    int quantity = 0;

    friend bool operator==(const LineItem&, const LineItem&) = default;
};

struct CartState {
    std::vector<LineItem> items;
    std::optional<std::string> coupon;

    friend bool operator==(const CartState&, const CartState&) = default;
};

struct AddItem {
    std::string sku;
    std::string name;
    std::int64_t unit_price_cents = 0;
    int quantity = 1;

    friend bool operator==(const AddItem&, const AddItem&) = default;
};

struct RemoveItem {
    std::string sku;

    friend bool operator==(const RemoveItem&, const RemoveItem&) = default;
};

struct SetQuantity {
    std::string sku;
    int quantity = 0;

    friend bool operator==(const SetQuantity&, const SetQuantity&) = default;
};

struct ApplyCoupon {
    std::optional<std::string> code;  // std::nullopt clears the coupon

    friend bool operator==(const ApplyCoupon&, const ApplyCoupon&) = default;
};

struct ClearCart {
    friend bool operator==(const ClearCart&, const ClearCart&) = default;
};

using CartAction = std::variant<AddItem, RemoveItem, SetQuantity, ApplyCoupon, ClearCart>;

enum class CartError {
    invalid_quantity,  // quantity < 1 on add, or negative on set
    invalid_price,     // unit price < 0
    unknown_sku,       // SetQuantity on a sku that is not in the cart
    unknown_coupon,    // coupon code not in coupons()
};

/// Read-only coupon table: code -> percent off. {"SAVE10": 10, "HALF": 50}.
const std::map<std::string, int>& coupons();

/// Pure reducer. Returns the next state; `state` and `action` are untouched.
///  AddItem: appends, or adds quantity to an existing sku (keeping its position,
///           name and price).
///  RemoveItem: removes the sku; unknown sku is a no-op success.
///  SetQuantity: 0 removes the line; negative -> invalid_quantity; unknown -> unknown_sku.
///  ApplyCoupon: sets or clears the coupon; unknown code -> unknown_coupon.
///  ClearCart: returns an empty CartState (no items, no coupon).
[[nodiscard]] std::expected<CartState, CartError> reduce_cart(const CartState& state,
                                                             const CartAction& action);

/// Sum of unit_price_cents * quantity over all lines.
[[nodiscard]] std::int64_t subtotal_cents(const CartState& state);

/// subtotal minus the coupon discount, where discount = subtotal * percent / 100
/// using integer (floored) division. No coupon means no discount.
[[nodiscard]] std::int64_t total_cents(const CartState& state);

/// Folds the actions over `initial` with reduce_cart, stopping at the first error.
[[nodiscard]] std::expected<CartState, CartError> run_actions(const CartState& initial,
                                                             std::span<const CartAction> actions);

}  // namespace cppref::functional
