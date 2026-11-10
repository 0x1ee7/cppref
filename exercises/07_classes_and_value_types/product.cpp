#include "product.hpp"

#include <stdexcept>
#include <string>

namespace cppref::classes {
namespace {

[[noreturn]] void not_implemented(const char* name) {
    throw std::logic_error(std::string("not implemented: ") + name);
}

}  // namespace

std::expected<Product, ProductError> Product::create(std::string, std::string, Money, int) {
    not_implemented("Product::create");
}

std::size_t Product::total_created() {
    not_implemented("Product::total_created");
}

std::string_view Product::sku() const {
    not_implemented("Product::sku");
}

std::string_view Product::name() const {
    not_implemented("Product::name");
}

const Money& Product::price() const {
    not_implemented("Product::price");
}

int Product::quantity() const {
    not_implemented("Product::quantity");
}

bool Product::in_stock() const {
    not_implemented("Product::in_stock");
}

std::expected<void, ProductError> Product::set_name(std::string_view) {
    not_implemented("Product::set_name");
}

void Product::set_price(Money) {
    not_implemented("Product::set_price");
}

std::expected<void, ProductError> Product::set_quantity(int) {
    not_implemented("Product::set_quantity");
}

std::expected<void, ProductError> Product::restock(int) {
    not_implemented("Product::restock");
}

}  // namespace cppref::classes
