// Engineer-owned. Part 1: a single-pass input view over an std::istream.
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

    /// Current line. Precondition: not at end.
    const std::string& operator*() const;
    LineIterator& operator++();
    void operator++(int);

    friend bool operator==(const LineIterator& it, std::default_sentinel_t);

private:
    friend class LineRange;
    explicit LineIterator(std::istream* in);

    // engineer: stream pointer, current line, end flag, plus a private advance() helper
};

/// Single-pass view over the lines of a stream. It does not own the stream, which must outlive
/// the view and its iterators. `begin()` reads the first line; each `++` reads one more.
/// Copies of the view share the stream, so only one traversal is possible.
class LineRange : public std::ranges::view_interface<LineRange> {
public:
    explicit LineRange(std::istream& in);

    LineIterator begin();
    [[nodiscard]] std::default_sentinel_t end() const;

private:
    // engineer: pointer to the stream, plus any extra state you need
};

}  // namespace cppref::iterators_and_ranges
