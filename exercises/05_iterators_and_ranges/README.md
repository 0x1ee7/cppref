# 05 · Iterators and Ranges

## Scenario

A log-processing service ingests server logs and must stream them without loading whole files
into memory. It reads lines lazily from streams, pages through a remote log API, writes results
in batches, computes moving statistics, and merges several time-ordered sources into one feed.
Every stage is a lazy, single-pass view or generator. Nothing is materialised until a consumer
asks for it.

## Parts

1. **Part1LineRange**: `LineRange`, a single-pass input view over an `std::istream`. It reads one
   line per `++` and never reads ahead.
2. **Part2Pipeline**: `LogRecord`, `parse_line` (returns `std::expected`), `collect_at_least`
   (a `views::filter` pipeline with line-numbered errors), and `first_error` (a borrowed-iterator
   return that is `dangling` for temporaries).
3. **Part3PagedRange**: `paginate`, a lazily fetched range over cursor-based pages. It detects
   cursor loops and supports resuming from a start cursor.
4. **Part4Generator**: `Generator<T>`, a minimal coroutine generator with a `promise_type`, a
   move-only handle, and exception propagation at `++` and `begin()`.
5. **Part5Batching**: `read_batches` (owns its source, opens lazily, releases on early destroy)
   and `write_in_batches` (pulls only what the current batch needs).
6. **Part6MovingAverage**: `moving_average`, a lazy window over any input range, taken by value
   and validated eagerly.
7. **Part7MergeSorted**: `merge_sorted`, a lazy k-way merge over `std::vector<Generator<LogRecord>>`
   using a heap, with stable tie-breaking and exception propagation.

## Concepts exercised

- Iterator categories and the input-iterator contract (`iterator_concept`, `operator++(int)`).
- `std::ranges::view_interface`, `std::default_sentinel_t`, and sentinels that differ from the iterator type.
- Borrowed ranges, `std::ranges::borrowed_iterator_t`, `std::ranges::dangling`.
- Lazy pipelines with `views::filter` and `views::transform`, and view ownership (`owning_view`, `views::all`).
- `std::expected` for recoverable parse errors; exceptions for stream and generator failures.
- C++20 coroutines: `promise_type`, `co_yield`, `initial_suspend`/`final_suspend`, and handle lifetime.
- Exception propagation across `co_yield` and coroutine resumption.
- Ownership of captured resources inside a coroutine frame (the source stream and the owned range).
- Heap-based k-way merge with `std::push_heap`/`std::pop_heap`.

## Run

Stubs (every test should fail):

```bat
tools\vsenv.cmd cmake --workflow --preset dev
tools\vsenv.cmd ctest --preset dev -R "05_iterators_and_ranges\.Part2"
tools\vsenv.cmd cmake --build --preset dev --target ex05_iterators_and_ranges_tests
```

Reference solutions:

```bat
tools\vsenv.cmd cmake --workflow --preset solutions
```

## Stretch (untested)

- Make `LineRange` a forward range by buffering a single line, then measure what breaks.
- Replace the `PagedRange` cursor set with a bounded hash set and reason about the memory bound.
- Add a `zip`-style merge of two generators and compare it with `merge_sorted`.
- Port `Generator` to `std::generator` once MSVC ships it, and compare the two designs.
