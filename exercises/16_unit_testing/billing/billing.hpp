// Billing module under test. PROVIDED: do not edit. The grader mutates billing.cpp.
#pragma once

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace billing {

// Money is integer cents. Rates are basis points: 10000 bps == 100%.
using Cents = std::int64_t;
inline constexpr std::int64_t kBpsDenominator = 10000;
inline constexpr int kDefaultNetDays = 30;

// ---- Errors -------------------------------------------------------------

class BillingError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

// Malformed invoice or line item.
class InvalidInvoice : public BillingError {
 public:
  using BillingError::BillingError;
};

// Retryable gateway failure (timeout, 503, ...).
class TransientGatewayError : public BillingError {
 public:
  using BillingError::BillingError;
};

// Terminal gateway failure (card declined). Never retried.
class PaymentDeclined : public BillingError {
 public:
  using BillingError::BillingError;
};

// Retries exhausted. The last TransientGatewayError is attached as a nested
// exception (std::throw_with_nested): inspect it with std::rethrow_if_nested.
class PaymentFailed : public BillingError {
 public:
  using BillingError::BillingError;
};

// ---- Money --------------------------------------------------------------

// amount * bps / 10000, rounded to the nearest cent, ties away from zero.
// Throws std::invalid_argument if amount < 0 or bps < 0.
Cents apply_bps(Cents amount, std::int64_t bps);

// Volume discount in bps: subtotal >= 100000 -> 1000 (10%),
// >= 50000 -> 500 (5%), >= 10000 -> 200 (2%), otherwise 0. Boundaries inclusive.
std::int64_t discount_bps(Cents subtotal);

// "12.34", "0.05", "0.00", "-2.50".
std::string format_money(Cents cents);

class LineItem {
 public:
  // Throws InvalidInvoice if sku is empty, unit_price < 0, or quantity < 1.
  LineItem(std::string sku, Cents unit_price, std::int64_t quantity);

  [[nodiscard]] const std::string& sku() const noexcept { return sku_; }
  [[nodiscard]] Cents unit_price() const noexcept { return unit_price_; }
  [[nodiscard]] std::int64_t quantity() const noexcept { return quantity_; }
  [[nodiscard]] Cents line_total() const { return unit_price_ * quantity_; }

 private:
  std::string sku_;
  Cents unit_price_;
  std::int64_t quantity_;
};

class Invoice {
 public:
  // Throws InvalidInvoice if lines is empty or tax_bps is outside [0, 10000).
  Invoice(std::string invoice_id, std::string customer_email,
          std::vector<LineItem> lines, std::chrono::year_month_day issued_on,
          std::int64_t tax_bps = 0, std::string currency = "USD");

  [[nodiscard]] const std::string& id() const noexcept { return id_; }
  [[nodiscard]] const std::string& customer_email() const noexcept { return email_; }
  [[nodiscard]] const std::vector<LineItem>& lines() const noexcept { return lines_; }
  [[nodiscard]] std::chrono::year_month_day issued_on() const noexcept { return issued_on_; }
  [[nodiscard]] std::int64_t tax_bps() const noexcept { return tax_bps_; }
  [[nodiscard]] const std::string& currency() const noexcept { return currency_; }

 private:
  std::string id_;
  std::string email_;
  std::vector<LineItem> lines_;
  std::chrono::year_month_day issued_on_;
  std::int64_t tax_bps_;
  std::string currency_;
};

struct Totals {
  Cents subtotal = 0;
  Cents discount = 0;
  Cents tax = 0;
  Cents total = 0;

  bool operator==(const Totals&) const = default;
};

// discount = apply_bps(subtotal, discount_bps(subtotal));
// tax      = apply_bps(subtotal - discount, tax_bps);
// total    = subtotal - discount + tax.
Totals invoice_totals(const Invoice& invoice);

// ---- Clock and dates ----------------------------------------------------

// Source of the current UTC instant. Production uses default_clock(); tests inject fakes.
class Clock {
 public:
  virtual ~Clock() = default;
  [[nodiscard]] virtual std::chrono::sys_seconds now() const = 0;
};

const Clock& default_clock();

// UTC calendar day containing instant.
std::chrono::year_month_day utc_date(std::chrono::sys_seconds instant);

// issued_on + net_days. Throws std::invalid_argument if net_days < 0.
std::chrono::year_month_day due_date(std::chrono::year_month_day issued_on,
                                     int net_days = kDefaultNetDays);

// due_date for an invoice issued on the UTC day of clock.now().
std::chrono::year_month_day issue_due_date(const Clock& clock,
                                           int net_days = kDefaultNetDays);

// True only strictly after due, judged by the UTC day of clock.now().
bool is_overdue(const Clock& clock, std::chrono::year_month_day due);

// ---- Ports --------------------------------------------------------------

class Gateway {
 public:
  virtual ~Gateway() = default;
  // Returns a transaction id. Throws TransientGatewayError (retryable) or
  // PaymentDeclined (terminal). amount is in cents.
  virtual std::string charge(Cents amount, const std::string& currency,
                             const std::string& token,
                             const std::string& idempotency_key) = 0;
};

class Notifier {
 public:
  virtual ~Notifier() = default;
  virtual void send(const std::string& to, const std::string& subject,
                    const std::string& body) = 0;
};

enum class LogLevel { debug, info, warning, error };

class Logger {
 public:
  virtual ~Logger() = default;
  virtual void log(LogLevel level, const std::string& message) = 0;
};

class Sleeper {
 public:
  virtual ~Sleeper() = default;
  virtual void sleep_for(std::chrono::milliseconds duration) = 0;
};

// Real sleeper: blocks the calling thread.
Sleeper& system_sleeper();

// ---- Charging -----------------------------------------------------------

struct RetryPolicy {
  int max_attempts = 4;
  std::chrono::milliseconds base_delay{1000};
  std::chrono::milliseconds max_delay{8000};
};

class PaymentService {
 public:
  // Collaborators are borrowed and must outlive the service.
  // Throws std::invalid_argument if max_attempts < 1 or a delay is negative.
  PaymentService(Gateway& gateway, Notifier& notifier, Logger& logger,
                 RetryPolicy policy = {}, Sleeper& sleeper = system_sleeper());

  // Delay after the failed 1-based attempt: min(base * 2^(attempt-1), max).
  [[nodiscard]] std::chrono::milliseconds backoff_delay(int attempt) const;

  // Charges the invoice total with the given token and returns the transaction id.
  //  - total <= 0: InvalidInvoice, gateway never called.
  //  - TransientGatewayError: logged at warning, retried after backoff_delay
  //    (no sleep after the last attempt); PaymentFailed when exhausted.
  //  - PaymentDeclined: logged at error, rethrown, no retry.
  //  - Other exceptions propagate without retry.
  //  - On success a receipt is sent. Receipt failures are logged at error
  //    and never fail the charge.
  std::string charge(const Invoice& invoice, const std::string& token);

 private:
  void send_receipt(const Invoice& invoice, const Totals& totals,
                    const std::string& transaction_id);

  Gateway& gateway_;
  Notifier& notifier_;
  Logger& logger_;
  RetryPolicy policy_;
  Sleeper& sleeper_;
};

// ---- CSV export ---------------------------------------------------------

inline constexpr std::array<std::string_view, 8> kExportFields = {
    "invoice_id", "customer_email", "issued_on", "currency",
    "subtotal",   "discount",       "tax",       "total"};

// Pull-based invoice stream. next() may throw mid-stream.
class InvoiceSource {
 public:
  virtual ~InvoiceSource() = default;
  virtual std::optional<Invoice> next() = 0;
};

class VectorInvoiceSource final : public InvoiceSource {
 public:
  explicit VectorInvoiceSource(std::vector<Invoice> invoices);
  std::optional<Invoice> next() override;

 private:
  std::vector<Invoice> invoices_;
  std::size_t next_ = 0;
};

// Writes the header and one row per invoice to "<path>.tmp", then renames it
// over path. Returns the number of invoice rows. On any exception: path is
// left untouched, the temp file is removed, and the exception propagates.
std::size_t export_invoices_csv(InvoiceSource& source,
                                const std::filesystem::path& path);

}  // namespace billing
