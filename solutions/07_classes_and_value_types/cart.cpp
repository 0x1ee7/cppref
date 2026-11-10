#include "cart.hpp"

#include <climits>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace cppref::classes {

ShoppingCart::ShoppingCart(Currency currency) : currency_(currency) {}

Currency ShoppingCart::currency() const {
    return currency_;
}

std::expected<void, CartError> ShoppingCart::add(const Product& product, int quantity) {
    if (quantity < 1) {
        return std::unexpected(CartError::invalid_quantity);
    }
    if (product.price().currency() != currency_) {
        return std::unexpected(CartError::currency_mismatch);
    }
    const auto units = static_cast<std::size_t>(INT_MAX);
    if (static_cast<std::size_t>(quantity) > units - size()) {
        return std::unexpected(CartError::overflow);
    }

    const std::size_t index = index_of(product.sku());
    if (index == kNoLine) {
        lines_.push_back(CartLine{std::string(product.sku()), quantity, product.price()});
    } else {
        lines_[index].quantity += quantity;
        lines_[index].unit_price = product.price();
    }
    return {};
}

std::expected<void, CartError> ShoppingCart::remove(std::string_view sku, int quantity) {
    if (quantity < 1) {
        return std::unexpected(CartError::invalid_quantity);
    }
    const std::size_t index = index_of(sku);
    if (index == kNoLine) {
        return std::unexpected(CartError::not_found);
    }
    CartLine& line = lines_[index];
    if (quantity > line.quantity) {
        return std::unexpected(CartError::insufficient_quantity);
    }
    line.quantity -= quantity;
    if (line.quantity == 0) {
        lines_.erase(lines_.begin() + static_cast<std::ptrdiff_t>(index));
    }
    return {};
}

Money ShoppingCart::total() const {
    Money sum = Money::zero(currency_);
    for (const CartLine& line : lines_) {
        sum = sum + line.unit_price * line.quantity;
    }
    return sum;
}

std::size_t ShoppingCart::size() const {
    std::size_t units = 0;
    for (const CartLine& line : lines_) {
        units += static_cast<std::size_t>(line.quantity);
    }
    return units;
}

bool ShoppingCart::empty() const {
    return lines_.empty();
}

bool ShoppingCart::contains(std::string_view sku) const {
    return index_of(sku) != kNoLine;
}

bool ShoppingCart::contains(const Product& product) const {
    return contains(product.sku());
}

int ShoppingCart::operator[](std::string_view sku) const {
    const std::size_t index = index_of(sku);
    if (index == kNoLine) {
        throw std::out_of_range("no such sku in cart: " + std::string(sku));
    }
    return lines_[index].quantity;
}

ShoppingCart::const_iterator ShoppingCart::begin() const {
    return lines_.begin();
}

ShoppingCart::const_iterator ShoppingCart::end() const {
    return lines_.end();
}

std::size_t ShoppingCart::index_of(std::string_view sku) const {
    for (std::size_t i = 0; i < lines_.size(); ++i) {
        if (lines_[i].sku == sku) {
            return i;
        }
    }
    return kNoLine;
}

std::ostream& operator<<(std::ostream& out, const ShoppingCart& cart) {
    return out << "ShoppingCart(currency='" << cart.currency_.code() << "', items=" << cart.size() << ')';
}

}  // namespace cppref::classes
