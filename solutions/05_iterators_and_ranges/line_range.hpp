// Part 1: a single-pass input view over an std::istream.
#pragma once

#include <cstddef>
#include <istream>
#include <iterator>
#include <ranges>
#include <string>

namespace cppref::iterators_and_ranges {

class LineRange;

/// Input iterator over the lines of a stream. Reads on demand: `*it` is the current line
/// (without its '\n'), `++it` extracts exactly one more line. Nothing is read ahead.
class LineIterator {
public:
    using difference_type = std::ptrdiff_t;
    using value_type = std::string;
    using iterator_concept = std::input_iterator_tag;
    using iterator_category = std::input_iterator_tag;

    /// Current line. Precondition: not at end.
    const std::string& operator*() const { return line_; }
    LineIterator& operator++() {
        advance();
        return *this;
    }
    void operator++(int) { advance(); }

    friend bool operator==(const LineIterator& it, std::default_sentinel_t) {
        return it.at_end_;
    }

private:
    friend class LineRange;
    explicit LineIterator(std::istream* in) : in_(in) {}

    void advance();

    std::istream* in_ = nullptr;
    std::string line_;
    bool at_end_ = true;
};

/// Single-pass view over the lines of a stream. It does not own the stream, which must outlive
/// the view and its iterators. `begin()` reads the first line; each `++` reads one more.
class LineRange : public std::ranges::view_interface<LineRange> {
public:
    LineRange() = default;
    explicit LineRange(std::istream& in) : in_(&in) {}

    LineIterator begin() {
        LineIterator it(in_);
        it.advance();
        return it;
    }
    [[nodiscard]] std::default_sentinel_t end() const { return {}; }

private:
    std::istream* in_ = nullptr;
};

}  // namespace cppref::iterators_and_ranges
