#include "line_range.hpp"

#include <istream>
#include <string>

namespace cppref::iterators_and_ranges {

void LineIterator::advance() {
    at_end_ = in_ == nullptr || !std::getline(*in_, line_);
}

}  // namespace cppref::iterators_and_ranges
