// Part 3 (Product) and Part 5 (ShoppingCart).
#include <gtest/gtest.h>

#include <climits>
#include <stdexcept>
#include <sstream>
#include <string>
#include <vector>

#include "cart.hpp"
#include "product.hpp"

namespace cc = cppref::classes;

namespace {

cc::Currency currency(std::string_view code) {
    return cc::Currency::parse(code).value();
}

cc::Money money(std::string_view text, std::string_view code = "USD") {
    return cc::Money::parse(text, currency(code)).value();
}

cc::Product product(std::string_view sku, std::string_view price, int quantity = 0,
                    std::string_view code = "USD") {
    return cc::Product::create(std::string(sku), "Widget", money(price, code), quantity).value();
}

template <typename T, typename E>
void expect_error(const std::expected<T, E>& result, E expected) {
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), expected);
}

}  // namespace

// ---------------------------------------------------------------- Part 3

TEST(Part3Product, CreateValidatesSku) {
    EXPECT_EQ(product("ABCD", "1.00").sku(), "ABCD");
    EXPECT_EQ(product("ABCD-1234", "1.00").sku(), "ABCD-1234");
    for (std::string_view bad : {"abc", "AB", "AB CD", "AB_CD", "ABCDEFGHIJKLMNOPQ"}) {
        expect_error(cc::Product::create(std::string(bad), "Widget", money("1.00"), 0),
                     cc::ProductError::invalid_sku);
    }
}

TEST(Part3Product, CreateTrimsAndValidatesName) {
    auto p = cc::Product::create("ABCD", "  Widget  ", money("1.00"), 0).value();
    EXPECT_EQ(p.name(), "Widget");
    expect_error(cc::Product::create("ABCD", "   ", money("1.00"), 0), cc::ProductError::invalid_name);
    expect_error(cc::Product::create("ABCD", std::string(101, 'x'), money("1.00"), 0),
                 cc::ProductError::invalid_name);
}

TEST(Part3Product, NegativeQuantityRejectedAndStockFlag) {
    expect_error(cc::Product::create("ABCD", "Widget", money("1.00"), -1), cc::ProductError::invalid_quantity);
    auto p = product("ABCD", "1.00", 0);
    EXPECT_FALSE(p.in_stock());
    EXPECT_EQ(p.quantity(), 0);
}

TEST(Part3Product, TotalCreatedCountsOnlySuccessfulCreates) {
    const auto before = cc::Product::total_created();
    (void)product("ABCD", "1.00");
    EXPECT_EQ(cc::Product::total_created(), before + 1);
    expect_error(cc::Product::create("bad", "Widget", money("1.00"), 0), cc::ProductError::invalid_sku);
    EXPECT_EQ(cc::Product::total_created(), before + 1);
}

TEST(Part3Product, SettersValidateAndLeaveStateUnchangedOnError) {
    auto p = product("ABCD", "1.00", 4);
    expect_error(p.set_name(""), cc::ProductError::invalid_name);
    EXPECT_EQ(p.name(), "Widget");
    expect_error(p.set_quantity(-1), cc::ProductError::invalid_quantity);
    EXPECT_EQ(p.quantity(), 4);

    ASSERT_TRUE(p.set_name("  Gadget ").has_value());
    EXPECT_EQ(p.name(), "Gadget");

    p.set_price(money("9.99", "EUR"));
    EXPECT_EQ(p.price().currency().code(), "EUR");
}

TEST(Part3Product, RestockAddsAndRejectsOverflow) {
    auto p = product("ABCD", "1.00", 2);
    expect_error(p.restock(0), cc::ProductError::invalid_quantity);
    ASSERT_TRUE(p.restock(5).has_value());
    EXPECT_EQ(p.quantity(), 7);
    EXPECT_TRUE(p.in_stock());

    ASSERT_TRUE(p.set_quantity(INT_MAX).has_value());
    expect_error(p.restock(1), cc::ProductError::overflow);
    EXPECT_EQ(p.quantity(), INT_MAX);
}

// ---------------------------------------------------------------- Part 5

TEST(Part5Cart, AddAccumulatesQuantityAndCountsUnits) {
    cc::ShoppingCart cart(currency("USD"));
    const auto widget = product("ABCD", "2.50");
    ASSERT_TRUE(cart.add(widget, 2).has_value());
    ASSERT_TRUE(cart.add(widget, 3).has_value());
    EXPECT_EQ(cart["ABCD"], 5);
    EXPECT_EQ(cart.size(), 5u);
    EXPECT_FALSE(cart.empty());
}

TEST(Part5Cart, TotalSumsLinesAndEmptyIsZero) {
    cc::ShoppingCart empty(currency("USD"));
    EXPECT_TRUE(empty.total().is_zero());
    EXPECT_TRUE(empty.empty());

    cc::ShoppingCart cart(currency("USD"));
    ASSERT_TRUE(cart.add(product("ABCD", "2.50"), 2).has_value());
    ASSERT_TRUE(cart.add(product("WXYZ", "1.00"), 1).has_value());
    EXPECT_EQ(cart.total(), money("6.00"));
}

TEST(Part5Cart, ReAddAdoptsCurrentProductPrice) {
    cc::ShoppingCart cart(currency("USD"));
    auto widget = product("ABCD", "1.00");
    ASSERT_TRUE(cart.add(widget, 2).has_value());
    widget.set_price(money("3.00"));
    ASSERT_TRUE(cart.add(widget, 1).has_value());
    EXPECT_EQ(cart.total(), money("9.00"));
}

TEST(Part5Cart, RemoveReducesAndDropsEmptyLines) {
    cc::ShoppingCart cart(currency("USD"));
    ASSERT_TRUE(cart.add(product("ABCD", "1.00"), 5).has_value());
    ASSERT_TRUE(cart.remove("ABCD", 2).has_value());
    EXPECT_EQ(cart["ABCD"], 3);
    ASSERT_TRUE(cart.remove("ABCD", 3).has_value());
    EXPECT_FALSE(cart.contains("ABCD"));
    EXPECT_TRUE(cart.empty());
}

TEST(Part5Cart, RemoveErrors) {
    cc::ShoppingCart cart(currency("USD"));
    ASSERT_TRUE(cart.add(product("ABCD", "1.00"), 2).has_value());
    expect_error(cart.remove("NOPE-1"), cc::CartError::not_found);
    expect_error(cart.remove("ABCD", 5), cc::CartError::insufficient_quantity);
    expect_error(cart.remove("ABCD", 0), cc::CartError::invalid_quantity);
    EXPECT_EQ(cart["ABCD"], 2);
}

TEST(Part5Cart, AddRejectsBadQuantityAndForeignCurrency) {
    cc::ShoppingCart cart(currency("USD"));
    expect_error(cart.add(product("ABCD", "1.00"), 0), cc::CartError::invalid_quantity);
    expect_error(cart.add(product("WXYZ", "1.00", 0, "EUR"), 1), cc::CartError::currency_mismatch);
    EXPECT_TRUE(cart.empty());
}

TEST(Part5Cart, IterationPreservesInsertionOrder) {
    cc::ShoppingCart cart(currency("USD"));
    ASSERT_TRUE(cart.add(product("BBBB", "1.00"), 1).has_value());
    ASSERT_TRUE(cart.add(product("AAAA", "1.00"), 4).has_value());

    std::vector<std::string> skus;
    for (const auto& line : cart) {
        skus.push_back(line.sku);
    }
    EXPECT_EQ(skus, (std::vector<std::string>{"BBBB", "AAAA"}));
}

TEST(Part5Cart, ContainsBySkuAndProduct) {
    cc::ShoppingCart cart(currency("USD"));
    const auto widget = product("ABCD", "1.00");
    ASSERT_TRUE(cart.add(widget, 1).has_value());
    EXPECT_TRUE(cart.contains("ABCD"));
    EXPECT_TRUE(cart.contains(widget));
    EXPECT_FALSE(cart.contains("WXYZ"));
}

TEST(Part5Cart, MissingSkuSubscriptThrows) {
    cc::ShoppingCart cart(currency("USD"));
    EXPECT_THROW((void)cart["NOPE-1"], std::out_of_range);
}

TEST(Part5Cart, StreamsReprLikeSummary) {
    cc::ShoppingCart cart(currency("USD"));
    ASSERT_TRUE(cart.add(product("ABCD", "1.00"), 3).has_value());
    std::ostringstream out;
    out << cart;
    EXPECT_EQ(out.str(), "ShoppingCart(currency='USD', items=3)");
}
