// Part 7: lazy k-way merge of time-ordered streams.
#pragma once

#include <vector>

#include "generator.hpp"
#include "log_records.hpp"

namespace cppref::iterators_and_ranges {

/// Lazily merges streams that are each sorted by `ts` into one stream sorted by `ts`.
/// Ties go to the stream with the lower index in `streams`.
Generator<LogRecord> merge_sorted(std::vector<Generator<LogRecord>> streams);

}  // namespace cppref::iterators_and_ranges
