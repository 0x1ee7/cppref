// Engineer-owned. Definitions for line_range.hpp.
#include "line_range.hpp"

#include <stdexcept>

namespace cppref::iterators_and_ranges {

LineIterator::LineIterator(std::istream*) {
    throw std::logic_error("not implemented: LineIterator");
}

const std::string& LineIterator::operator*() const {
    throw std::logic_error("not implemented: LineIterator::operator*");
}

LineIterator& LineIterator::operator++() {
    throw std::logic_error("not implemented: LineIterator::operator++");
}

void LineIterator::operator++(int) {
    throw std::logic_error("not implemented: LineIterator::operator++(int)");
}

bool operator==(const LineIterator&, std::default_sentinel_t) {
    throw std::logic_error("not implemented: LineIterator == sentinel");
}

LineRange::LineRange(std::istream&) {
    throw std::logic_error("not implemented: LineRange");
}

LineIterator LineRange::begin() {
    throw std::logic_error("not implemented: LineRange::begin");
}

std::default_sentinel_t LineRange::end() const {
    throw std::logic_error("not implemented: LineRange::end");
}

}  // namespace cppref::iterators_and_ranges
