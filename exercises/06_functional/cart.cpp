#include "cart.hpp"

#include <stdexcept>

// Part 3 · Redux-style cart reducer. Stub: implement every function.

namespace cppref::functional {

const std::map<std::string, int>& coupons() {
    throw std::logic_error("not implemented: coupons");
}

std::expected<CartState, CartError> reduce_cart(const CartState&, const CartAction&) {
    throw std::logic_error("not implemented: reduce_cart");
}

std::int64_t subtotal_cents(const CartState&) {
    throw std::logic_error("not implemented: subtotal_cents");
}

std::int64_t total_cents(const CartState&) {
    throw std::logic_error("not implemented: total_cents");
}

std::expected<CartState, CartError> run_actions(const CartState&, std::span<const CartAction>) {
    throw std::logic_error("not implemented: run_actions");
}

}  // namespace cppref::functional
