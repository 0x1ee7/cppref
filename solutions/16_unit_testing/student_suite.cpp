// Reference suite for billing/. Solutions only.
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "billing/billing.hpp"

namespace {

namespace bl = billing;
namespace chr = std::chrono;
namespace fs = std::filesystem;

using testing::_;
using testing::AllOf;
using testing::HasSubstr;
using testing::InSequence;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;
using testing::Throw;

// ---- Test doubles -----------------------------------------------------------

class MockGateway : public bl::Gateway {
 public:
  MOCK_METHOD(std::string, charge,
              (bl::Cents, const std::string&, const std::string&, const std::string&),
              (override));
};

class MockNotifier : public bl::Notifier {
 public:
  MOCK_METHOD(void, send, (const std::string&, const std::string&, const std::string&),
              (override));
};

class MockLogger : public bl::Logger {
 public:
  MOCK_METHOD(void, log, (bl::LogLevel, const std::string&), (override));
};

class MockSleeper : public bl::Sleeper {
 public:
  MOCK_METHOD(void, sleep_for, (chr::milliseconds), (override));
};

class MockInvoiceSource : public bl::InvoiceSource {
 public:
  MOCK_METHOD(std::optional<bl::Invoice>, next, (), (override));
};

class FakeClock final : public bl::Clock {
 public:
  explicit FakeClock(chr::sys_seconds now) : now_(now) {}
  chr::sys_seconds now() const override { return now_; }

 private:
  chr::sys_seconds now_;
};

// Source that yields one invoice, then fails mid-stream.
class FailingSource final : public bl::InvoiceSource {
 public:
  std::optional<bl::Invoice> next() override {
    if (calls_++ == 0) {
      return bl::Invoice("INV-1", "a@example.com",
                         {bl::LineItem("SKU-1", 10000, 1)},
                         chr::year_month_day{chr::year{2026}, chr::November, chr::day{10}});
    }
    throw std::runtime_error("disk gone");
  }

 private:
  int calls_ = 0;
};

// ---- Helpers ----------------------------------------------------------------

chr::year_month_day ymd(int y, unsigned m, unsigned d) {
  return chr::year_month_day{chr::year{y}, chr::month{m}, chr::day{d}};
}

chr::sys_seconds at_utc(chr::year_month_day day, int hour, int minute) {
  return chr::sys_seconds{chr::sys_days{day}} + chr::hours{hour} + chr::minutes{minute};
}

bl::Invoice invoice_with(std::vector<bl::LineItem> lines, std::int64_t tax_bps = 0,
                         std::string currency = "USD") {
  return bl::Invoice("INV-1", "a@example.com", std::move(lines), ymd(2026, 11, 10),
                     tax_bps, std::move(currency));
}

bl::Invoice single_line_invoice(bl::Cents price, std::int64_t tax_bps = 0,
                                std::string currency = "USD") {
  return invoice_with({bl::LineItem("SKU-1", price, 1)}, tax_bps, std::move(currency));
}

constexpr const char* kCsvHeader =
    "invoice_id,customer_email,issued_on,currency,subtotal,discount,tax,total\n";

// ---- Part 1: money and invoice totals ---------------------------------------

TEST(Part1Money, ApplyBpsRoundsHalfUp) {
  EXPECT_EQ(bl::apply_bps(1, 5000), 1);  // 0.5 cent rounds up
  EXPECT_EQ(bl::apply_bps(4999, 1), 0);  // 0.4999 cent rounds down
  EXPECT_EQ(bl::apply_bps(5000, 1), 1);
}

TEST(Part1Money, ApplyBpsZeroAndFullRate) {
  EXPECT_EQ(bl::apply_bps(12345, 0), 0);
  EXPECT_EQ(bl::apply_bps(12345, 10000), 12345);
}

TEST(Part1Money, ApplyBpsRejectsNegativeOperands) {
  EXPECT_THROW(bl::apply_bps(-1, 100), std::invalid_argument);
  EXPECT_THROW(bl::apply_bps(100, -1), std::invalid_argument);
}

TEST(Part1Money, FormatMoneyRendersCents) {
  EXPECT_EQ(bl::format_money(1234), "12.34");
  EXPECT_EQ(bl::format_money(5), "0.05");
  EXPECT_EQ(bl::format_money(0), "0.00");
  EXPECT_EQ(bl::format_money(-250), "-2.50");
}

TEST(Part1Money, LineItemRejectsInvalidFields) {
  EXPECT_THROW(bl::LineItem("", 100, 1), bl::InvalidInvoice);
  EXPECT_THROW(bl::LineItem("SKU", -1, 1), bl::InvalidInvoice);
  EXPECT_THROW(bl::LineItem("SKU", 100, 0), bl::InvalidInvoice);
}

TEST(Part1Money, LineItemAllowsFreeItemsAndMultiplies) {
  EXPECT_NO_THROW(bl::LineItem("FREE", 0, 1));
  EXPECT_EQ(bl::LineItem("SKU", 250, 4).line_total(), 1000);
}

TEST(Part1Money, InvoiceRejectsEmptyLinesAndBadTax) {
  EXPECT_THROW(invoice_with({}), bl::InvalidInvoice);
  EXPECT_THROW(single_line_invoice(100, 10000), bl::InvalidInvoice);
  EXPECT_THROW(single_line_invoice(100, -1), bl::InvalidInvoice);
  EXPECT_NO_THROW(single_line_invoice(100, 9999));
}

TEST(Part1Money, InvoiceExposesFields) {
  const auto invoice = invoice_with({bl::LineItem("A", 100, 2)});
  EXPECT_EQ(invoice.id(), "INV-1");
  EXPECT_EQ(invoice.customer_email(), "a@example.com");
  EXPECT_EQ(invoice.currency(), "USD");
  EXPECT_EQ(invoice.lines().size(), 1u);
}

TEST(Part1Totals, WorkedExampleWithDiscountAndTax) {
  // 40000 + 2 * 20000 = 80000; 5% tier discount = 4000; taxable 76000; tax 10% = 7600.
  const auto invoice = invoice_with({bl::LineItem("A", 40000, 1), bl::LineItem("B", 20000, 2)}, 1000);
  EXPECT_EQ(bl::invoice_totals(invoice), (bl::Totals{80000, 4000, 7600, 83600}));
}

TEST(Part1Totals, TaxIsComputedOnDiscountedAmount) {
  // subtotal 10000 -> discount 200 -> taxable 9800 -> tax 980 -> total 10780.
  EXPECT_EQ(bl::invoice_totals(single_line_invoice(10000, 1000)),
            (bl::Totals{10000, 200, 980, 10780}));
}

TEST(Part1Totals, NoDiscountBelowFirstTier) {
  EXPECT_EQ(bl::invoice_totals(single_line_invoice(4999)), (bl::Totals{4999, 0, 0, 4999}));
}

// ---- Part2: discount tiers --------------------------------------------------

using TierCase = std::pair<bl::Cents, std::int64_t>;

class Part2DiscountTiers : public testing::TestWithParam<TierCase> {};

TEST_P(Part2DiscountTiers, RateAtTierBoundaries) {
  const auto [subtotal, expected_bps] = GetParam();
  EXPECT_EQ(bl::discount_bps(subtotal), expected_bps);
}

INSTANTIATE_TEST_SUITE_P(
    Boundaries, Part2DiscountTiers,
    testing::ValuesIn(std::vector<TierCase>{{0, 0},
                                            {9999, 0},
                                            {10000, 200},
                                            {49999, 200},
                                            {50000, 500},
                                            {99999, 500},
                                            {100000, 1000},
                                            {1000000, 1000}}));

TEST(Part2DiscountTotals, DiscountAppliedToSubtotal) {
  // 5% of 50000 = 2500; no tax; total 47500.
  EXPECT_EQ(bl::invoice_totals(single_line_invoice(50000)),
            (bl::Totals{50000, 2500, 0, 47500}));
}

// ---- Part3: clock and dates -------------------------------------------------

TEST(Part3Clock, UtcDateUsesUtcCalendarDay) {
  EXPECT_EQ(bl::utc_date(at_utc(ymd(2026, 11, 10), 23, 30)), ymd(2026, 11, 10));
  EXPECT_EQ(bl::utc_date(at_utc(ymd(2026, 11, 11), 0, 0)), ymd(2026, 11, 11));
}

TEST(Part3Clock, DueDateAddsNetDays) {
  EXPECT_EQ(bl::due_date(ymd(2026, 11, 10)), ymd(2026, 12, 10));  // default 30 days
  EXPECT_EQ(bl::due_date(ymd(2026, 11, 10), 0), ymd(2026, 11, 10));
}

TEST(Part3Clock, DueDateCrossesLeapDayAndYearEnd) {
  EXPECT_EQ(bl::due_date(ymd(2028, 2, 15), 30), ymd(2028, 3, 16));
  EXPECT_EQ(bl::due_date(ymd(2026, 12, 31), 1), ymd(2027, 1, 1));
}

TEST(Part3Clock, DueDateRejectsNegativeNetDays) {
  EXPECT_THROW(bl::due_date(ymd(2026, 11, 10), -1), std::invalid_argument);
}

TEST(Part3Clock, IssueDueDateUsesUtcDayOfClock) {
  const FakeClock clock(at_utc(ymd(2026, 11, 10), 23, 30));
  EXPECT_EQ(bl::issue_due_date(clock), ymd(2026, 12, 10));
}

TEST(Part3Clock, NotOverdueOnDueDay) {
  const FakeClock clock(at_utc(ymd(2026, 11, 10), 23, 59));
  EXPECT_FALSE(bl::is_overdue(clock, ymd(2026, 11, 10)));
}

TEST(Part3Clock, OverdueOnlyAfterDueDay) {
  EXPECT_TRUE(bl::is_overdue(FakeClock(at_utc(ymd(2026, 11, 11), 0, 0)), ymd(2026, 11, 10)));
  EXPECT_FALSE(bl::is_overdue(FakeClock(at_utc(ymd(2026, 11, 9), 12, 0)), ymd(2026, 11, 10)));
}

// ---- Part4: charging and retries -------------------------------------------

class Part4Charging : public testing::Test {
 protected:
  bl::PaymentService make_service(bl::RetryPolicy policy = {}) {
    return bl::PaymentService(gateway, notifier, logger, policy, sleeper);
  }

  StrictMock<MockGateway> gateway;
  NiceMock<MockNotifier> notifier;
  NiceMock<MockLogger> logger;
  StrictMock<MockSleeper> sleeper;
};

TEST_F(Part4Charging, ChargesDiscountedTotalWithTokenAndInvoiceKey) {
  // 10000 minus 2% = 9800.
  EXPECT_CALL(gateway, charge(bl::Cents{9800}, "EUR", "tok-1", "INV-1"))
      .WillOnce(Return(std::string("txn-1")));
  auto service = make_service();
  EXPECT_EQ(service.charge(single_line_invoice(10000, 0, "EUR"), "tok-1"), "txn-1");
}

TEST_F(Part4Charging, ZeroTotalIsRejectedBeforeGateway) {
  auto service = make_service();
  EXPECT_THROW(service.charge(single_line_invoice(0), "tok"), bl::InvalidInvoice);
}

TEST_F(Part4Charging, TransientFailureIsRetriedAfterBackoff) {
  {
    InSequence seq;
    EXPECT_CALL(gateway, charge(_, _, _, _)).WillOnce(Throw(bl::TransientGatewayError("503")));
    EXPECT_CALL(sleeper, sleep_for(chr::milliseconds{1000}));
    EXPECT_CALL(gateway, charge(_, _, _, _)).WillOnce(Return(std::string("txn-2")));
  }
  auto service = make_service();
  EXPECT_EQ(service.charge(single_line_invoice(10000), "tok"), "txn-2");
}

TEST_F(Part4Charging, ExhaustedRetriesThrowAfterBackoffSchedule) {
  EXPECT_CALL(gateway, charge(_, _, _, _))
      .Times(4)
      .WillRepeatedly(Throw(bl::TransientGatewayError("503")));
  {
    InSequence seq;
    EXPECT_CALL(sleeper, sleep_for(chr::milliseconds{1000}));
    EXPECT_CALL(sleeper, sleep_for(chr::milliseconds{2000}));
    EXPECT_CALL(sleeper, sleep_for(chr::milliseconds{4000}));
  }
  auto service = make_service();
  EXPECT_THROW(service.charge(single_line_invoice(10000), "tok"), bl::PaymentFailed);
}

TEST_F(Part4Charging, PaymentFailedCarriesLastTransientErrorAsNestedCause) {
  EXPECT_CALL(gateway, charge(_, _, _, _))
      .Times(2)
      .WillRepeatedly(Throw(bl::TransientGatewayError("503 from gateway")));
  EXPECT_CALL(sleeper, sleep_for(chr::milliseconds{1000}));
  auto service = make_service(bl::RetryPolicy{.max_attempts = 2});

  bool inner_seen = false;
  try {
    service.charge(single_line_invoice(10000), "tok");
    ADD_FAILURE() << "expected PaymentFailed";
  } catch (const bl::PaymentFailed& err) {
    try {
      std::rethrow_if_nested(err);
    } catch (const bl::TransientGatewayError& inner) {
      inner_seen = true;
      EXPECT_STREQ(inner.what(), "503 from gateway");
    }
  }
  EXPECT_TRUE(inner_seen);
}

TEST_F(Part4Charging, DeclinedPaymentIsNotRetried) {
  EXPECT_CALL(gateway, charge(_, _, _, _))
      .WillOnce(Throw(bl::PaymentDeclined("card declined")));
  auto service = make_service();
  EXPECT_THROW(service.charge(single_line_invoice(10000), "tok"), bl::PaymentDeclined);
}

TEST_F(Part4Charging, UnknownGatewayErrorPropagatesWithoutRetry) {
  EXPECT_CALL(gateway, charge(_, _, _, _)).WillOnce(Throw(std::runtime_error("socket reset")));
  auto service = make_service();
  EXPECT_THROW(service.charge(single_line_invoice(10000), "tok"), std::runtime_error);
}

TEST_F(Part4Charging, SingleAttemptPolicyNeverSleeps) {
  EXPECT_CALL(gateway, charge(_, _, _, _)).WillOnce(Throw(bl::TransientGatewayError("503")));
  auto service = make_service(bl::RetryPolicy{.max_attempts = 1});
  EXPECT_THROW(service.charge(single_line_invoice(10000), "tok"), bl::PaymentFailed);
}

TEST_F(Part4Charging, BackoffDoublesFromBaseAndCapsAtDefaultMax) {
  auto service = make_service();  // base 1000 ms, max 8000 ms
  EXPECT_EQ(service.backoff_delay(1), chr::milliseconds{1000});
  EXPECT_EQ(service.backoff_delay(2), chr::milliseconds{2000});
  EXPECT_EQ(service.backoff_delay(3), chr::milliseconds{4000});
  EXPECT_EQ(service.backoff_delay(4), chr::milliseconds{8000});
  EXPECT_EQ(service.backoff_delay(6), chr::milliseconds{8000});
  EXPECT_EQ(service.backoff_delay(10), chr::milliseconds{8000});
}

TEST_F(Part4Charging, BackoffNeverExceedsMaxWhenMaxIsNotPowerOfTwoMultiple) {
  // Doubling 1000 overshoots 5000 to 8000; the delay must still be capped.
  auto service = make_service(bl::RetryPolicy{.max_attempts = 4,
                                              .base_delay = chr::milliseconds{1000},
                                              .max_delay = chr::milliseconds{5000}});
  EXPECT_EQ(service.backoff_delay(3), chr::milliseconds{4000});
  EXPECT_EQ(service.backoff_delay(4), chr::milliseconds{5000});
  EXPECT_EQ(service.backoff_delay(5), chr::milliseconds{5000});
}

TEST_F(Part4Charging, ConstructorRejectsInvalidPolicy) {
  EXPECT_THROW(make_service(bl::RetryPolicy{.max_attempts = 0}), std::invalid_argument);
  EXPECT_THROW(make_service(bl::RetryPolicy{.base_delay = chr::milliseconds{-1}}),
               std::invalid_argument);
}

// ---- Part5: receipts and logging --------------------------------------------

class Part5Receipts : public testing::Test {
 protected:
  bl::PaymentService make_service(bl::RetryPolicy policy = {}) {
    return bl::PaymentService(gateway, notifier, logger, policy, sleeper);
  }

  StrictMock<MockGateway> gateway;
  StrictMock<MockNotifier> notifier;
  StrictMock<MockLogger> logger;
  StrictMock<MockSleeper> sleeper;
};

TEST_F(Part5Receipts, ReceiptGoesToCustomerWithTotalAndTransaction) {
  EXPECT_CALL(gateway, charge(_, _, _, _)).WillOnce(Return(std::string("txn-9")));
  EXPECT_CALL(notifier,
              send("a@example.com", "Receipt for invoice INV-1",
                   AllOf(HasSubstr("Total: 98.00 EUR"), HasSubstr("txn-9"))));
  auto service = make_service();
  service.charge(single_line_invoice(10000, 0, "EUR"), "tok");
}

TEST_F(Part5Receipts, ReceiptFailureIsLoggedAndDoesNotFailCharge) {
  EXPECT_CALL(gateway, charge(_, _, _, _)).WillOnce(Return(std::string("txn-3")));
  EXPECT_CALL(notifier, send(_, _, _)).WillOnce(Throw(std::runtime_error("smtp down")));
  EXPECT_CALL(logger, log(bl::LogLevel::error,
                          HasSubstr("receipt failed for invoice INV-1: smtp down")));
  auto service = make_service();
  EXPECT_EQ(service.charge(single_line_invoice(10000), "tok"), "txn-3");
}

TEST_F(Part5Receipts, TransientFailureLogsWarningWithAttemptCount) {
  EXPECT_CALL(gateway, charge(_, _, _, _))
      .WillOnce(Throw(bl::TransientGatewayError("503")))
      .WillOnce(Return(std::string("txn-4")));
  EXPECT_CALL(sleeper, sleep_for(_));
  EXPECT_CALL(notifier, send(_, _, _));
  EXPECT_CALL(logger, log(bl::LogLevel::warning,
                          HasSubstr("charge failed (attempt 1 of 4): 503")));
  auto service = make_service();
  service.charge(single_line_invoice(10000), "tok");
}

TEST_F(Part5Receipts, DeclinedPaymentLogsError) {
  EXPECT_CALL(gateway, charge(_, _, _, _)).WillOnce(Throw(bl::PaymentDeclined("card declined")));
  EXPECT_CALL(logger, log(bl::LogLevel::error,
                          HasSubstr("payment declined for invoice INV-1: card declined")));
  auto service = make_service();
  EXPECT_THROW(service.charge(single_line_invoice(10000), "tok"), bl::PaymentDeclined);
}

// ---- Part6: CSV export ------------------------------------------------------

class Part6Export : public testing::Test {
 protected:
  void SetUp() override {
    const auto* info = testing::UnitTest::GetInstance()->current_test_info();
    dir_ = fs::temp_directory_path() /
           (std::string("cppref16-") + info->test_suite_name() + "-" + info->name() + "-" +
            std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::error_code ignored;
    fs::remove_all(dir_, ignored);
    fs::create_directories(dir_);
    path_ = dir_ / "invoices.csv";
  }

  void TearDown() override {
    std::error_code ignored;
    fs::remove_all(dir_, ignored);
  }

  fs::path temp_path() const {
    fs::path temp = path_;
    temp += ".tmp";
    return temp;
  }

  void write_text(const fs::path& file, const std::string& text) const {
    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    out << text;
  }

  std::string read_text(const fs::path& file) const {
    std::ifstream in(file, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
  }

  fs::path dir_;
  fs::path path_;
};

TEST_F(Part6Export, WritesHeaderAndOneRowPerInvoice) {
  bl::VectorInvoiceSource source{std::vector<bl::Invoice>{single_line_invoice(10000, 0, "EUR")}};
  EXPECT_EQ(bl::export_invoices_csv(source, path_), 1u);
  EXPECT_EQ(read_text(path_),
            std::string(kCsvHeader) +
                "INV-1,a@example.com,2026-11-10,EUR,100.00,2.00,0.00,98.00\n");
}

TEST_F(Part6Export, RowsCarryTaxComputedAfterDiscount) {
  bl::VectorInvoiceSource source{std::vector<bl::Invoice>{single_line_invoice(10000, 1000, "EUR")}};
  bl::export_invoices_csv(source, path_);
  EXPECT_THAT(read_text(path_),
              HasSubstr("INV-1,a@example.com,2026-11-10,EUR,100.00,2.00,9.80,107.80\n"));
}

TEST_F(Part6Export, QuotesFieldsContainingCommasAndQuotes) {
  bl::VectorInvoiceSource source{std::vector<bl::Invoice>{
      bl::Invoice("A,\"B\"", "a@example.com", {bl::LineItem("SKU-1", 10000, 1)},
                  ymd(2026, 11, 10))}};
  bl::export_invoices_csv(source, path_);
  EXPECT_THAT(read_text(path_),
              HasSubstr(R"("A,""B""",a@example.com,2026-11-10,USD,100.00,2.00,0.00,98.00)"));
}

TEST_F(Part6Export, EmptySourceWritesHeaderOnly) {
  bl::VectorInvoiceSource source{std::vector<bl::Invoice>{}};
  EXPECT_EQ(bl::export_invoices_csv(source, path_), 0u);
  EXPECT_EQ(read_text(path_), kCsvHeader);
}

TEST_F(Part6Export, PullsUntilSourceIsExhausted) {
  MockInvoiceSource source;
  {
    InSequence seq;
    EXPECT_CALL(source, next())
        .WillOnce(Return(std::optional<bl::Invoice>(single_line_invoice(100))));
    EXPECT_CALL(source, next())
        .WillOnce(Return(std::optional<bl::Invoice>(single_line_invoice(200))));
    EXPECT_CALL(source, next()).WillOnce(Return(std::optional<bl::Invoice>{}));
  }
  EXPECT_EQ(bl::export_invoices_csv(source, path_), 2u);
}

TEST_F(Part6Export, SuccessfulExportReplacesExistingFileAndLeavesNoTemp) {
  write_text(path_, "old contents\n");
  bl::VectorInvoiceSource source{std::vector<bl::Invoice>{}};
  bl::export_invoices_csv(source, path_);
  EXPECT_EQ(read_text(path_), kCsvHeader);
  EXPECT_FALSE(fs::exists(temp_path()));
}

TEST_F(Part6Export, FailedExportKeepsPreviousFileAndRemovesTemp) {
  write_text(path_, "previous\n");
  FailingSource source;
  EXPECT_THROW(bl::export_invoices_csv(source, path_), std::runtime_error);
  EXPECT_EQ(read_text(path_), "previous\n");
  EXPECT_FALSE(fs::exists(temp_path()));
}

}  // namespace
