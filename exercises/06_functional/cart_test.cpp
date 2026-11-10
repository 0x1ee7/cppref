#include "cart.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace fn = cppref::functional;

namespace {

fn::LineItem line(std::string sku, std::string name, std::int64_t price, int qty) {
    return fn::LineItem{std::move(sku), std::move(name), price, qty};
}

fn::CartState cart_with(std::vector<fn::LineItem> items,
                        std::optional<std::string> coupon = std::nullopt) {
    return fn::CartState{std::move(items), std::move(coupon)};
}

}  // namespace

TEST(Part3CartReducer, AddItemAppendsNewLine) {
    auto r = fn::reduce_cart(fn::CartState{}, fn::CartAction{fn::AddItem{"A1", "Apple", 150, 2}});
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->items, (std::vector<fn::LineItem>{line("A1", "Apple", 150, 2)}));
}

TEST(Part3CartReducer, AddItemMergesQuantityKeepingPosition) {
    auto s = cart_with({line("A1", "Apple", 150, 1), line("B2", "Bread", 300, 1)});
    auto r = fn::reduce_cart(s, fn::CartAction{fn::AddItem{"A1", "Other", 999, 3}});
    ASSERT_TRUE(r.has_value());
    ASSERT_EQ(r->items.size(), 2u);
    EXPECT_EQ(r->items[0], line("A1", "Apple", 150, 4));  // name and price of the first add win
    EXPECT_EQ(r->items[1], line("B2", "Bread", 300, 1));
    EXPECT_EQ(s.items[0].quantity, 1);  // input unchanged
}

TEST(Part3CartReducer, AddItemRejectsInvalidQuantityAndPrice) {
    auto zero_qty = fn::reduce_cart(fn::CartState{}, fn::CartAction{fn::AddItem{"A1", "Apple", 150, 0}});
    ASSERT_FALSE(zero_qty.has_value());
    EXPECT_EQ(zero_qty.error(), fn::CartError::invalid_quantity);

    auto neg_price = fn::reduce_cart(fn::CartState{}, fn::CartAction{fn::AddItem{"A1", "Apple", -1, 1}});
    ASSERT_FALSE(neg_price.has_value());
    EXPECT_EQ(neg_price.error(), fn::CartError::invalid_price);
}

TEST(Part3CartReducer, RemoveItemDropsLineAndIgnoresUnknownSku) {
    auto s = cart_with({line("A1", "Apple", 150, 1), line("B2", "Bread", 300, 1)});
    auto r = fn::reduce_cart(s, fn::CartAction{fn::RemoveItem{"A1"}});
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->items, (std::vector<fn::LineItem>{line("B2", "Bread", 300, 1)}));

    auto unknown = fn::reduce_cart(s, fn::CartAction{fn::RemoveItem{"ZZ"}});
    ASSERT_TRUE(unknown.has_value());
    EXPECT_EQ(*unknown, s);
}

TEST(Part3CartReducer, SetQuantityZeroRemovesTheLine) {
    auto s = cart_with({line("A1", "Apple", 150, 1)});
    auto set = fn::reduce_cart(s, fn::CartAction{fn::SetQuantity{"A1", 5}});
    ASSERT_TRUE(set.has_value());
    EXPECT_EQ(set->items[0].quantity, 5);

    auto cleared = fn::reduce_cart(s, fn::CartAction{fn::SetQuantity{"A1", 0}});
    ASSERT_TRUE(cleared.has_value());
    EXPECT_TRUE(cleared->items.empty());
}

TEST(Part3CartReducer, SetQuantityValidatesQuantityBeforeSku) {
    auto s = cart_with({line("A1", "Apple", 150, 1)});

    auto negative = fn::reduce_cart(s, fn::CartAction{fn::SetQuantity{"A1", -1}});
    ASSERT_FALSE(negative.has_value());
    EXPECT_EQ(negative.error(), fn::CartError::invalid_quantity);

    // Quantity is checked first, so a negative quantity on an unknown sku is
    // still reported as invalid_quantity.
    auto both_wrong = fn::reduce_cart(s, fn::CartAction{fn::SetQuantity{"ZZ", -1}});
    ASSERT_FALSE(both_wrong.has_value());
    EXPECT_EQ(both_wrong.error(), fn::CartError::invalid_quantity);

    auto unknown = fn::reduce_cart(s, fn::CartAction{fn::SetQuantity{"ZZ", 2}});
    ASSERT_FALSE(unknown.has_value());
    EXPECT_EQ(unknown.error(), fn::CartError::unknown_sku);
}

TEST(Part3CartReducer, ApplyCouponSetsKnownCodeClearsOnNullopt) {
    auto s = cart_with({line("A1", "Apple", 150, 1)});

    auto set = fn::reduce_cart(s, fn::CartAction{fn::ApplyCoupon{std::string{"SAVE10"}}});
    ASSERT_TRUE(set.has_value());
    EXPECT_EQ(set->coupon, std::optional<std::string>{"SAVE10"});

    auto cleared = fn::reduce_cart(*set, fn::CartAction{fn::ApplyCoupon{std::nullopt}});
    ASSERT_TRUE(cleared.has_value());
    EXPECT_FALSE(cleared->coupon.has_value());

    auto bogus = fn::reduce_cart(s, fn::CartAction{fn::ApplyCoupon{std::string{"BOGUS"}}});
    ASSERT_FALSE(bogus.has_value());
    EXPECT_EQ(bogus.error(), fn::CartError::unknown_coupon);
}

TEST(Part3CartReducer, ClearCartResetsItemsAndCoupon) {
    auto s = cart_with({line("A1", "Apple", 150, 1)}, std::string{"HALF"});
    auto r = fn::reduce_cart(s, fn::CartAction{fn::ClearCart{}});
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, fn::CartState{});
}

TEST(Part3CartReducer, TotalsApplyFlooredPercentDiscount) {
    auto items = std::vector<fn::LineItem>{line("A", "a", 1000, 2), line("B", "b", 333, 1)};
    EXPECT_EQ(fn::subtotal_cents(cart_with(items)), 2333);
    EXPECT_EQ(fn::total_cents(cart_with(items)), 2333);
    EXPECT_EQ(fn::total_cents(cart_with(items, std::string{"SAVE10"})), 2333 - 233);  // 233.3 -> 233
    EXPECT_EQ(fn::total_cents(cart_with(items, std::string{"HALF"})), 2333 - 1166);   // 1166.5 -> 1166
}

TEST(Part3CartReducer, CouponTableMapsCodesToPercent) {
    EXPECT_EQ(fn::coupons().size(), 2u);
    EXPECT_EQ(fn::coupons().at("SAVE10"), 10);
    EXPECT_EQ(fn::coupons().at("HALF"), 50);
}

TEST(Part3CartReducer, RunActionsFoldsAndStopsAtFirstError) {
    std::vector<fn::CartAction> actions{
        fn::AddItem{"A1", "Apple", 150, 2},
        fn::SetQuantity{"A1", 5},
        fn::ApplyCoupon{std::string{"SAVE10"}},
    };
    auto r = fn::run_actions(fn::CartState{}, std::span<const fn::CartAction>(actions));
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->items, (std::vector<fn::LineItem>{line("A1", "Apple", 150, 5)}));
    EXPECT_EQ(r->coupon, std::optional<std::string>{"SAVE10"});

    std::vector<fn::CartAction> failing{
        fn::AddItem{"A1", "Apple", 150, 1},
        fn::ApplyCoupon{std::string{"NOPE"}},
        fn::AddItem{"B2", "Bread", 300, 1},
    };
    auto err = fn::run_actions(fn::CartState{}, std::span<const fn::CartAction>(failing));
    ASSERT_FALSE(err.has_value());
    EXPECT_EQ(err.error(), fn::CartError::unknown_coupon);
}
