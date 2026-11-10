// Billing module implementation. PROVIDED: do not edit (the grader mutates it).
#include "billing/billing.hpp"

#include <algorithm>
#include <exception>
#include <format>
#include <fstream>
#include <system_error>
#include <thread>
#include <utility>

namespace billing {
namespace chr = std::chrono;

namespace {

class SystemClock final : public Clock {
 public:
  chr::sys_seconds now() const override {
    return chr::floor<chr::seconds>(chr::system_clock::now());
  }
};

class ThreadSleeper final : public Sleeper {
 public:
  void sleep_for(chr::milliseconds duration) override {
    std::this_thread::sleep_for(duration);
  }
};

// RFC 4180: quote when the field contains a comma, quote, or line break.
std::string csv_escape(std::string_view field) {
  if (field.find_first_of(",\"\r\n") == std::string_view::npos) {
    return std::string(field);
  }
  std::string out = "\"";
  for (char c : field) {
    if (c == '"') out += '"';
    out += c;
  }
  out += '"';
  return out;
}

std::string iso_date(chr::year_month_day ymd) {
  return std::format("{:04}-{:02}-{:02}", static_cast<int>(ymd.year()),
                     static_cast<unsigned>(ymd.month()),
                     static_cast<unsigned>(ymd.day()));
}

std::string invoice_row(const Invoice& invoice) {
  const Totals t = invoice_totals(invoice);
  const std::array<std::string, 8> fields = {
      invoice.id(),
      invoice.customer_email(),
      iso_date(invoice.issued_on()),
      invoice.currency(),
      format_money(t.subtotal),
      format_money(t.discount), format_money(t.tax),
      format_money(t.total)};
  std::string row;
  for (std::size_t i = 0; i < fields.size(); ++i) {
    if (i != 0) row += ',';
    row += csv_escape(fields[i]);
  }
  return row;
}

}  // namespace

// ---- Money --------------------------------------------------------------

Cents apply_bps(Cents amount, std::int64_t bps) {
  if (amount < 0 || bps < 0) {
    throw std::invalid_argument("apply_bps: operands must be non-negative");
  }
  return (amount * bps + kBpsDenominator / 2) / kBpsDenominator;
}

std::int64_t discount_bps(Cents subtotal) {
  if (subtotal >= 100000) return 1000;
  if (subtotal >= 50000) return 500;
  if (subtotal >= 10000) return 200;
  return 0;
}

std::string format_money(Cents cents) {
  const bool negative = cents < 0;
  const auto magnitude = static_cast<std::uint64_t>(negative ? -cents : cents);
  return std::format("{}{}.{:02}", negative ? "-" : "", magnitude / 100,
                     magnitude % 100);
}

LineItem::LineItem(std::string sku, Cents unit_price, std::int64_t quantity)
    : sku_(std::move(sku)), unit_price_(unit_price), quantity_(quantity) {
  if (sku_.empty()) throw InvalidInvoice("line item sku must not be empty");
  if (unit_price_ < 0) throw InvalidInvoice("line item unit price must not be negative");
  if (quantity_ < 1) throw InvalidInvoice("line item quantity must be at least 1");
}

Invoice::Invoice(std::string invoice_id, std::string customer_email,
                 std::vector<LineItem> lines, chr::year_month_day issued_on,
                 std::int64_t tax_bps, std::string currency)
    : id_(std::move(invoice_id)),
      email_(std::move(customer_email)),
      lines_(std::move(lines)),
      issued_on_(issued_on),
      tax_bps_(tax_bps),
      currency_(std::move(currency)) {
  if (lines_.empty()) throw InvalidInvoice("invoice must have at least one line");
  if (tax_bps_ < 0 || tax_bps_ >= kBpsDenominator) {
    throw InvalidInvoice("tax rate must be in [0, 1)");
  }
}

Totals invoice_totals(const Invoice& invoice) {
  Totals t;
  for (const LineItem& line : invoice.lines()) {
    t.subtotal += line.line_total();
  }
  t.discount = apply_bps(t.subtotal, discount_bps(t.subtotal));
  t.tax = apply_bps(t.subtotal - t.discount, invoice.tax_bps());
  t.total = t.subtotal - t.discount + t.tax;
  return t;
}

// ---- Clock and dates ----------------------------------------------------

const Clock& default_clock() {
  static const SystemClock clock;
  return clock;
}

chr::year_month_day utc_date(chr::sys_seconds instant) {
  return chr::year_month_day{chr::floor<chr::days>(instant)};
}

chr::year_month_day due_date(chr::year_month_day issued_on, int net_days) {
  if (net_days < 0) throw std::invalid_argument("net_days must be non-negative");
  return chr::year_month_day{chr::sys_days{issued_on} + chr::days{net_days}};
}

chr::year_month_day issue_due_date(const Clock& clock, int net_days) {
  return due_date(utc_date(clock.now()), net_days);
}

bool is_overdue(const Clock& clock, chr::year_month_day due) {
  const chr::year_month_day today = utc_date(clock.now());
  return today > due;
}

// ---- Charging -----------------------------------------------------------

Sleeper& system_sleeper() {
  static ThreadSleeper sleeper;
  return sleeper;
}

PaymentService::PaymentService(Gateway& gateway, Notifier& notifier, Logger& logger,
                               RetryPolicy policy, Sleeper& sleeper)
    : gateway_(gateway),
      notifier_(notifier),
      logger_(logger),
      policy_(policy),
      sleeper_(sleeper) {
  if (policy_.max_attempts < 1) {
    throw std::invalid_argument("max_attempts must be at least 1");
  }
  if (policy_.base_delay < chr::milliseconds::zero() ||
      policy_.max_delay < chr::milliseconds::zero()) {
    throw std::invalid_argument("retry delays must be non-negative");
  }
}

chr::milliseconds PaymentService::backoff_delay(int attempt) const {
  // Doubling stops once the cap is reached, so the value never overflows.
  chr::milliseconds delay = policy_.base_delay;
  for (int i = 1; i < attempt && delay < policy_.max_delay; ++i) {
    delay *= 2;
  }
  return std::min(delay, policy_.max_delay);
}

std::string PaymentService::charge(const Invoice& invoice, const std::string& token) {
  const Totals totals = invoice_totals(invoice);
  if (totals.total <= 0) {
    throw InvalidInvoice("invoice total must be positive");
  }

  for (int attempt = 1;; ++attempt) {
    try {
      std::string transaction_id = gateway_.charge(
          totals.total, invoice.currency(), token, invoice.id());
      send_receipt(invoice, totals, transaction_id);
      return transaction_id;
    } catch (const TransientGatewayError& err) {
      logger_.log(LogLevel::warning,
                  std::format("charge failed (attempt {} of {}): {}", attempt,
                              policy_.max_attempts, err.what()));
      if (attempt >= policy_.max_attempts) {
        std::throw_with_nested(PaymentFailed(
            std::format("payment failed after {} attempts", attempt)));
      }
      sleeper_.sleep_for(backoff_delay(attempt));
    } catch (const PaymentDeclined& err) {
      logger_.log(LogLevel::error,
                  std::format("payment declined for invoice {}: {}", invoice.id(),
                              err.what()));
      throw;
    }
  }
}

void PaymentService::send_receipt(const Invoice& invoice, const Totals& totals,
                                  const std::string& transaction_id) {
  const std::string subject = std::format("Receipt for invoice {}", invoice.id());
  const std::string body = std::format(
      "Thank you for your payment.\nInvoice: {}\nTotal: {} {}\nTransaction: {}\n",
      invoice.id(), format_money(totals.total), invoice.currency(), transaction_id);
  try {
    notifier_.send(invoice.customer_email(), subject, body);
  } catch (const std::exception& err) {
    logger_.log(LogLevel::error,
                std::format("receipt failed for invoice {}: {}", invoice.id(), err.what()));
  }
}

// ---- CSV export ---------------------------------------------------------

VectorInvoiceSource::VectorInvoiceSource(std::vector<Invoice> invoices)
    : invoices_(std::move(invoices)) {}

std::optional<Invoice> VectorInvoiceSource::next() {
  if (next_ >= invoices_.size()) return std::nullopt;
  return invoices_[next_++];
}

std::size_t export_invoices_csv(InvoiceSource& source,
                                const std::filesystem::path& path) {
  namespace fs = std::filesystem;
  fs::path temp = path;
  temp += ".tmp";
  std::size_t rows = 0;
  try {
    std::ofstream out(temp, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot open " + temp.string());
    for (std::size_t i = 0; i < kExportFields.size(); ++i) {
      if (i != 0) out << ',';
      out << kExportFields[i];
    }
    out << '\n';
    while (std::optional<Invoice> invoice = source.next()) {
      out << invoice_row(*invoice) << '\n';
      ++rows;
    }
    out.close();
    if (out.fail()) throw std::runtime_error("failed writing " + temp.string());
  } catch (...) {
    std::error_code ignored;
    fs::remove(temp, ignored);
    throw;
  }
  fs::rename(temp, path);
  return rows;
}

}  // namespace billing
