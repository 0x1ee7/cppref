// Part 8: window-function reports (RANK, running SUM OVER), cancelled orders excluded.

#include "model.hpp"
#include "reports.hpp"
#include "repositories.hpp"
#include "sqlite_wrapper.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

namespace db = cppref::database_sqlite;
using cppref::database_sqlite::testing::TestDb;
using cppref::database_sqlite::testing::at;
using cppref::database_sqlite::testing::seed_catalogue;

namespace {

// Catalogue: WIDGET 1000c, GADGET 2550c, GIZMO 325c. Customers: Alice 1, Bob 2, Carol 3, Dan 4.
// Orders: o1 Alice t100 paid    WIDGET x2 + GADGET x1
//         o2 Alice t200 shipped GIZMO x4
//         o3 Alice t300 cancelled WIDGET x10
//         o4 Alice t400 pending WIDGET x1
//         o5 Bob   t150 paid    GADGET x2
//         o6 Dan   t500 cancelled GIZMO x3
void seed_reports(TestDb& test) {
    seed_catalogue(test.db());
    db::CustomerRepository customers(test.db());
    const auto alice = customers.add("alice@x.test", "Alice", at(1)).id;
    const auto bob = customers.add("bob@x.test", "Bob", at(1)).id;
    customers.add("carol@x.test", "Carol", at(1));
    const auto dan = customers.add("dan@x.test", "Dan", at(1)).id;

    db::OrderRepository orders(test.db());
    const std::vector<db::OrderLineInput> o1{{"WIDGET", 2}, {"GADGET", 1}};
    const std::vector<db::OrderLineInput> o2{{"GIZMO", 4}};
    const std::vector<db::OrderLineInput> o3{{"WIDGET", 10}};
    const std::vector<db::OrderLineInput> o4{{"WIDGET", 1}};
    const std::vector<db::OrderLineInput> o5{{"GADGET", 2}};
    const std::vector<db::OrderLineInput> o6{{"GIZMO", 3}};

    const auto id1 = orders.place(alice, at(100), o1);
    const auto id2 = orders.place(alice, at(200), o2);
    const auto id3 = orders.place(alice, at(300), o3);
    orders.place(alice, at(400), o4);
    const auto id5 = orders.place(bob, at(150), o5);
    const auto id6 = orders.place(dan, at(500), o6);
    orders.set_status(id1, "paid");
    orders.set_status(id2, "shipped");
    orders.set_status(id3, "cancelled");
    orders.set_status(id5, "paid");
    orders.set_status(id6, "cancelled");
}

}  // namespace

TEST(Part8Reports, UnitsSoldByProductExcludeCancelledAndSortByUnits) {
    TestDb test;
    seed_reports(test);
    const std::vector<db::ProductSales> expected{
        {"GIZMO", 4, 1},
        {"GADGET", 3, 2},
        {"WIDGET", 3, 2},
    };
    EXPECT_EQ(db::units_sold_by_product(test.db()), expected);
}

TEST(Part8Reports, RevenueByCustomerIsExactCents) {
    TestDb test;
    seed_reports(test);
    const std::vector<db::CustomerRevenue> expected{
        {1, "Alice", 3, db::Money{6850}},  // 2*1000 + 2550 + 4*325 + 1000
        {2, "Bob", 1, db::Money{5100}},    // 2*2550
    };
    EXPECT_EQ(db::revenue_by_customer(test.db()), expected);
}

TEST(Part8Reports, RankUsesRankWithTiesAndIncludesZeroOrderCustomers) {
    TestDb test;
    seed_reports(test);
    const std::vector<db::RankedCustomer> expected{
        {1, "Alice", 3, 1},
        {2, "Bob", 1, 2},
        {3, "Carol", 0, 3},
        {4, "Dan", 0, 3},  // only a cancelled order: counts as zero
    };
    EXPECT_EQ(db::rank_customers_by_orders(test.db()), expected);
}

TEST(Part8Reports, RunningUnitsAccumulatePerCustomerOverNonCancelledOrders) {
    TestDb test;
    seed_reports(test);
    const std::vector<db::RunningUnits> alice{
        {1, at(100), 3, 3},
        {2, at(200), 4, 7},
        {4, at(400), 1, 8},  // order 3 was cancelled and is skipped
    };
    EXPECT_EQ(db::customer_running_units(test.db(), 1), alice);

    const std::vector<db::RunningUnits> bob{{5, at(150), 2, 2}};
    EXPECT_EQ(db::customer_running_units(test.db(), 2), bob);
}

TEST(Part8Reports, CustomersWithoutLiveOrdersHaveNoRunningRows) {
    TestDb test;
    seed_reports(test);
    EXPECT_TRUE(db::customer_running_units(test.db(), 3).empty());  // Carol: no orders
    EXPECT_TRUE(db::customer_running_units(test.db(), 4).empty());  // Dan: cancelled only
}

TEST(Part8Reports, EmptyDatabaseYieldsEmptyReports) {
    TestDb test;
    EXPECT_TRUE(db::units_sold_by_product(test.db()).empty());
    EXPECT_TRUE(db::revenue_by_customer(test.db()).empty());
    EXPECT_TRUE(db::rank_customers_by_orders(test.db()).empty());
    EXPECT_TRUE(db::customer_running_units(test.db(), 1).empty());
}
