#include "product.hpp"

#include <atomic>
#include <limits>
#include <utility>

namespace cppref::classes {
namespace {

constexpr std::size_t kMinSku = 4;
constexpr std::size_t kMaxSku = 16;
constexpr std::size_t kMaxName = 100;

std::atomic<std::size_t> g_created{0};

bool is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

std::string_view trim(std::string_view text) {
    while (!text.empty() && is_space(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_space(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

bool valid_sku(std::string_view sku) {
    if (sku.size() < kMinSku || sku.size() > kMaxSku) {
        return false;
    }
    for (char c : sku) {
        const bool allowed = (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
        if (!allowed) {
            return false;
        }
    }
    return true;
}

std::expected<std::string, ProductError> normalize_name(std::string_view name) {
    const auto trimmed = trim(name);
    if (trimmed.empty() || trimmed.size() > kMaxName) {
        return std::unexpected(ProductError::invalid_name);
    }
    return std::string(trimmed);
}

}  // namespace

std::expected<Product, ProductError> Product::create(std::string sku, std::string name, Money price, int quantity) {
    if (!valid_sku(sku)) {
        return std::unexpected(ProductError::invalid_sku);
    }
    auto normalized = normalize_name(name);
    if (!normalized) {
        return std::unexpected(normalized.error());
    }
    if (quantity < 0) {
        return std::unexpected(ProductError::invalid_quantity);
    }
    Product product(std::move(sku), std::move(*normalized), std::move(price), quantity);
    g_created.fetch_add(1, std::memory_order_relaxed);
    return product;
}

std::size_t Product::total_created() {
    return g_created.load(std::memory_order_relaxed);
}

std::string_view Product::sku() const {
    return sku_;
}

std::string_view Product::name() const {
    return name_;
}

const Money& Product::price() const {
    return price_;
}

int Product::quantity() const {
    return quantity_;
}

bool Product::in_stock() const {
    return quantity_ > 0;
}

std::expected<void, ProductError> Product::set_name(std::string_view name) {
    auto normalized = normalize_name(name);
    if (!normalized) {
        return std::unexpected(normalized.error());
    }
    name_ = std::move(*normalized);
    return {};
}

void Product::set_price(Money price) {
    price_ = std::move(price);
}

std::expected<void, ProductError> Product::set_quantity(int quantity) {
    if (quantity < 0) {
        return std::unexpected(ProductError::invalid_quantity);
    }
    quantity_ = quantity;
    return {};
}

std::expected<void, ProductError> Product::restock(int count) {
    if (count < 1) {
        return std::unexpected(ProductError::invalid_quantity);
    }
    if (quantity_ > std::numeric_limits<int>::max() - count) {
        return std::unexpected(ProductError::overflow);
    }
    quantity_ += count;
    return {};
}

}  // namespace cppref::classes
