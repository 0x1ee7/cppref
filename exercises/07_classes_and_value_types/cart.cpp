#include "cart.hpp"

#include <ostream>
#include <stdexcept>
#include <string>

namespace cppref::classes {
namespace {

[[noreturn]] void not_implemented(const char* name) {
    throw std::logic_error(std::string("not implemented: ") + name);
}

}  // namespace

ShoppingCart::ShoppingCart(Currency) {
    not_implemented("ShoppingCart::ShoppingCart");
}

Currency ShoppingCart::currency() const {
    not_implemented("ShoppingCart::currency");
}

std::expected<void, CartError> ShoppingCart::add(const Product&, int) {
    not_implemented("ShoppingCart::add");
}

std::expected<void, CartError> ShoppingCart::remove(std::string_view, int) {
    not_implemented("ShoppingCart::remove");
}

Money ShoppingCart::total() const {
    not_implemented("ShoppingCart::total");
}

std::size_t ShoppingCart::size() const {
    not_implemented("ShoppingCart::size");
}

bool ShoppingCart::empty() const {
    not_implemented("ShoppingCart::empty");
}

bool ShoppingCart::contains(std::string_view) const {
    not_implemented("ShoppingCart::contains(sku)");
}

bool ShoppingCart::contains(const Product&) const {
    not_implemented("ShoppingCart::contains(Product)");
}

int ShoppingCart::operator[](std::string_view) const {
    not_implemented("ShoppingCart::operator[]");
}

ShoppingCart::const_iterator ShoppingCart::begin() const {
    not_implemented("ShoppingCart::begin");
}

ShoppingCart::const_iterator ShoppingCart::end() const {
    not_implemented("ShoppingCart::end");
}

std::ostream& operator<<(std::ostream&, const ShoppingCart&) {
    not_implemented("operator<<(ostream, ShoppingCart)");
}

}  // namespace cppref::classes
