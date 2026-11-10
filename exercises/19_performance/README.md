# 19 — Performance: allocation budgets and data layout

## Scenario

You own the hot path of an ingest service: it parses access logs at line rate, builds
response text for a metrics exporter, passes request-scoped form data through a per-request
arena, and aggregates sensor samples in batches. Performance regressions here show up as
allocator pressure, not as wrong answers, so every function in this exercise has an
allocation budget that the tests enforce.

The test executable replaces the global `operator new` and `operator delete` (see
`alloc_hook_test.cpp`) and counts calls on the current thread. `perf_support_test.hpp` holds
the `AllocScope` helper and instrumented `Probe` elements that count copies, moves and
lifetimes. Each allocation assertion runs only after the functional assertion, so a stub fails
on behaviour first.

## Files

| File | Role |
| --- | --- |
| `log_parser.hpp/.cpp` | Part 1 — zero-allocation line parsing |
| `formatting.hpp/.cpp` | Part 2 — single-allocation string building |
| `inline_containers.hpp` | Part 3 — `InlineVector`, `SmallString` |
| `move_ops.hpp` | Part 4 — `drain_front`, `emplace_n`, `append_strong` |
| `form_arena.hpp/.cpp` | Part 5 — form parsing into a `pmr` arena |
| `layout.hpp/.cpp` | Part 6 — `Sample` layout, columns, aggregation |
| `alloc_hook_test.cpp` | Global allocation counter (test-only) |
| `perf_support_test.hpp` | `AllocScope`, `Probe`, `Ledger`, `ids_of` (test-only) |
| `part*_test.cpp` | Tests, one file per Part |

## Part 1 — Zero-allocation log parsing

`parse_line` turns one log line into a `LogLine` of `string_view`s into the input. The grammar
is `<timestamp> <LEVEL> <component> status=<u32> latency_us=<u64>[ <message>]`. Errors report
a column: the offending token offset, or the digits offset for `bad_number`. `count_levels`
walks a multi-line buffer, strips one trailing `\r`, skips blank lines and tallies levels.
Neither function allocates.

Concepts: `string_view` lifetimes, `std::from_chars` for strict numeric parsing,
`std::expected` for error reporting, zero-copy views.

## Part 2 — Single-allocation string building

`join_csv` reserves the exact output size up front, so it makes at most one allocation (none
for an empty span). `format_access_line` makes at most one allocation: it sizes the output with
`std::formatted_size`, reserves, then formats. `format_access_line_into` writes into a
caller-provided buffer with `std::format_to_n` and returns either a view or the required size.

Concepts: `std::format` family, `formatted_size`, `format_to_n`, reserve-then-append,
caller-owned buffers.

## Part 3 — Inline-storage containers

`InlineVector<T, N>` keeps up to `N` elements in the object and spills to one heap block,
doubling its capacity. Moved-from vectors are empty. A spilled move steals the heap block.
`push_back` and `emplace_back` give the strong guarantee: a failed spill leaves the vector
unchanged, and `emplace_back` builds the new element before relocating so arguments that refer
to an existing element stay valid. `SmallString<N>` follows the same storage policy for
characters.

Concepts: aligned raw storage (`alignas`, `std::byte`), placement `construct_at`/`destroy_at`,
`std::launder`, `move_if_noexcept`, exception-safe growth, rule of five.

## Part 4 — Move semantics and reallocation policy

`drain_front` moves the first `n` elements out of a vector (no copies) and returns them in a
reserved vector. `emplace_n` reserves once for the whole batch, so no reallocation happens
inside the loop. `append_strong` stages copies first, so a throwing copy leaves the destination
unchanged; the final move into the destination uses reserved capacity.

Concepts: `std::movable`/`std::copyable` constraints, `make_move_iterator`, `reserve` before
append, the strong exception guarantee, counting copies vs moves with instrumented types.

## Part 5 — Request-scoped parsing with `std::pmr`

`parse_form` parses `application/x-www-form-urlencoded` bodies into a
`std::pmr::vector<FormField>`. Keys and values are `std::pmr::string` allocated from the
caller's arena, so a request costs one arena reset instead of many heap frees. The result keeps
the arena after it is moved. Exhausting the arena throws `std::bad_alloc`.

Concepts: `std::pmr::monotonic_buffer_resource`, `polymorphic_allocator`, uses-allocator
construction (`allocator_arg_t`), `std::expected` for decode errors, arena lifetime.

## Part 6 — Layout and columnar aggregation

`Sample` is 24 bytes because its members are ordered by alignment, not by declaration
convenience. `to_columns` splits a batch into three contiguous columns with at most three
allocations. `summarize_by_sensor` works only on the columns: it sorts a row-index array, then
computes one summary per sensor in at most three allocations regardless of sample count.

Concepts: struct padding and `sizeof`/`alignof`, struct-of-arrays layout, cache-friendly
passes, allocation budgets that do not grow with input size.

## Concepts exercised

- Allocation budgets enforced by a counting `operator new` in tests
- `string_view` and arena-backed views; no hidden copies
- `std::expected` for recoverable parse errors
- `std::format`, `formatted_size`, `format_to_n`
- Placement construction, `std::launder`, `move_if_noexcept`, strong guarantee
- Move-only and move-vs-copy observability through instrumented `Probe` types
- `std::pmr` arenas and uses-allocator construction
- Struct layout (`sizeof`, `alignof`) and struct-of-arrays aggregation

## Run commands

Windows (prefix every command with `tools\vsenv.cmd`):

```
tools\vsenv.cmd cmake --workflow --preset dev
tools\vsenv.cmd ctest --preset dev -R "19_performance\.Part2"
tools\vsenv.cmd cmake --build --preset dev --target ex19_performance_tests
tools\vsenv.cmd cmake --workflow --preset solutions
```

Benchmarking note: the allocation tests count calls, not time. Timing comparisons belong in a
separate benchmark with a noise floor (see the `perf-ab-testing` skill); do not add wall-clock
assertions to these tests.

## Stretch ideas (untested)

- Replace `InlineVector` growth with a fixed capacity policy that returns `std::expected` on
  overflow instead of spilling; compare the two in an allocation-counting benchmark.
- Add a `std::pmr::unsynchronized_pool_resource` variant for `parse_form` and measure reuse.
- Make `format_access_line` use `std::to_chars` directly and measure against `std::format`.
- Add a struct-of-arrays variant for `Sample` with `kind` and `flags` in a bit-packed column.
