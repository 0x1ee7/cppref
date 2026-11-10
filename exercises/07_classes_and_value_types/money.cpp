#include "money.hpp"

#include <ostream>
#include <string>

namespace cppref::classes {
namespace {

[[noreturn]] void not_implemented(const char* name) {
    throw std::logic_error(std::string("not implemented: ") + name);
}

}  // namespace

std::expected<Currency, MoneyError> Currency::parse(std::string_view) {
    not_implemented("Currency::parse");
}

std::string_view Currency::code() const {
    not_implemented("Currency::code");
}

int Currency::minor_digits() const {
    not_implemented("Currency::minor_digits");
}

std::expected<Money, MoneyError> Money::from_minor(std::int64_t, Currency) {
    not_implemented("Money::from_minor");
}

std::expected<Money, MoneyError> Money::parse(std::string_view, Currency) {
    not_implemented("Money::parse");
}

Money Money::zero(Currency) {
    not_implemented("Money::zero");
}

Currency Money::currency() const {
    not_implemented("Money::currency");
}

bool Money::is_zero() const {
    not_implemented("Money::is_zero");
}

bool Money::is_negative() const {
    not_implemented("Money::is_negative");
}

bool Money::is_positive() const {
    not_implemented("Money::is_positive");
}

Money Money::rounded() const {
    not_implemented("Money::rounded");
}

std::expected<std::vector<Money>, MoneyError> Money::allocate(int) const {
    not_implemented("Money::allocate");
}

Money operator+(const Money&, const Money&) {
    not_implemented("operator+(Money, Money)");
}

Money operator-(const Money&, const Money&) {
    not_implemented("operator-(Money, Money)");
}

Money operator-(const Money&) {
    not_implemented("operator-(Money)");
}

Money operator*(const Money&, std::int64_t) {
    not_implemented("operator*(Money, int64)");
}

Money operator*(std::int64_t, const Money&) {
    not_implemented("operator*(int64, Money)");
}

bool operator==(const Money&, const Money&) {
    not_implemented("operator==(Money, Money)");
}

std::strong_ordering operator<=>(const Money&, const Money&) {
    not_implemented("operator<=>(Money, Money)");
}

std::ostream& operator<<(std::ostream&, const Money&) {
    not_implemented("operator<<(ostream, Money)");
}

}  // namespace cppref::classes

std::size_t std::hash<cppref::classes::Money>::operator()(const cppref::classes::Money&) const {
    throw std::logic_error("not implemented: std::hash<Money>::operator()");
}
