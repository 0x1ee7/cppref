#include "cart.hpp"

#include "variant_utils.hpp"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace cppref::functional {

namespace {

using Result = std::expected<CartState, CartError>;

}  // namespace

const std::map<std::string, int>& coupons() {
    static const std::map<std::string, int> table{{"SAVE10", 10}, {"HALF", 50}};
    return table;
}

std::expected<CartState, CartError> reduce_cart(const CartState& state, const CartAction& action) {
    return std::visit(
        overloaded{
            [&state](const AddItem& add) -> Result {
                if (add.quantity < 1) {
                    return std::unexpected(CartError::invalid_quantity);
                }
                if (add.unit_price_cents < 0) {
                    return std::unexpected(CartError::invalid_price);
                }
                CartState next = state;
                auto it = std::ranges::find(next.items, add.sku, &LineItem::sku);
                if (it != next.items.end()) {
                    it->quantity += add.quantity;  // keep the first name and price
                } else {
                    next.items.push_back(
                        LineItem{add.sku, add.name, add.unit_price_cents, add.quantity});
                }
                return next;
            },
            [&state](const RemoveItem& remove) -> Result {
                CartState next = state;
                std::erase_if(next.items,
                              [&remove](const LineItem& item) { return item.sku == remove.sku; });
                return next;
            },
            [&state](const SetQuantity& set) -> Result {
                if (set.quantity < 0) {
                    return std::unexpected(CartError::invalid_quantity);
                }
                CartState next = state;
                auto it = std::ranges::find(next.items, set.sku, &LineItem::sku);
                if (it == next.items.end()) {
                    return std::unexpected(CartError::unknown_sku);
                }
                if (set.quantity == 0) {
                    next.items.erase(it);
                } else {
                    it->quantity = set.quantity;
                }
                return next;
            },
            [&state](const ApplyCoupon& apply) -> Result {
                CartState next = state;
                if (!apply.code) {
                    next.coupon.reset();
                } else if (!coupons().contains(*apply.code)) {
                    return std::unexpected(CartError::unknown_coupon);
                } else {
                    next.coupon = apply.code;
                }
                return next;
            },
            [](const ClearCart&) -> Result { return CartState{}; },
        },
        action);
}

std::expected<CartState, CartError> run_actions(const CartState& initial,
                                                std::span<const CartAction> actions) {
    CartState state = initial;
    for (const CartAction& action : actions) {
        auto next = reduce_cart(state, action);
        if (!next) {
            return std::unexpected(next.error());
        }
        state = std::move(*next);
    }
    return state;
}

std::int64_t subtotal_cents(const CartState& state) {
    std::int64_t total = 0;
    for (const LineItem& item : state.items) {
        total += item.unit_price_cents * item.quantity;
    }
    return total;
}

std::int64_t total_cents(const CartState& state) {
    const std::int64_t subtotal = subtotal_cents(state);
    if (!state.coupon) {
        return subtotal;
    }
    const auto& table = coupons();
    auto it = table.find(*state.coupon);
    if (it == table.end()) {
        return subtotal;
    }
    // Floor division: the discount never rounds in the customer's favour.
    const std::int64_t discount = subtotal * it->second / 100;
    return subtotal - discount;
}

}  // namespace cppref::functional
