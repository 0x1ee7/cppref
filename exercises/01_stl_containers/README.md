# 01 — STL containers and value semantics

Scenario: you are on the platform team for a telemetry and order service. Five small components need the right container, the right ownership model, and the right exception and lifetime guarantees: a sensor ingest filter, a TTL+LRU response cache, a per-client sliding-window rate limiter, a streaming top-K sales report, layered configuration, inventory reconciliation, and volume pricing with SLA banding.

The tests are the spec. Read the Part's section, then its tests in `part<N>_*_test.cpp`.

## Part 1 — Ingest filtering (`ingest.hpp`)

- `drop_invalid`: in-place erase-remove (or `std::erase_if`) of NaN values and negative timestamps; returns the count removed.
- `select_sensor`: copy matches in order, reserving the exact count.
- `split_fields`: split on a separator, keeping empty fields; the returned views alias the input.
- `hottest_sensor`: max over non-NaN values, earlier reading wins ties; returned view aliases the reading's storage.

Concepts: `erase_if`, `reserve`, `string_view` aliasing and lifetime, `optional<string_view>`, NaN comparison.

## Part 2 — TTL + LRU cache (`ttl_cache.hpp`)

Header-only class template `TtlCache<K, V, Hash>`. Bounded LRU with per-entry expiry. The clock is injected (`MonotonicClock`, a `std::function`), so tests control time.

- Expired iff `now >= written_at + ttl`. Expired entries are purged before capacity is enforced.
- `get` promotes to most-recent and counts hits/misses; `contains`, `keys`, and `size` have no side effects.

Concepts: `std::list` splice for O(1) recency updates, `unordered_map` of list iterators, iterator invalidation, `std::optional` returns, injected clocks, gMock for a collaborator.

## Part 3 — Sliding-window rate limiter (`rate_limiter.hpp`)

Per-client limit of N requests in a trailing window. Denied requests are not recorded. Queries never create state.

- `allow` / `remaining` / `retry_after` / `prune`.
- `retry_after` is the time until the oldest live event leaves the window.

Concepts: `std::deque` as a time-ordered queue, transparent hashing (`is_transparent`) so `string_view` lookups don't allocate, `const` queries, `std::chrono` arithmetic.

## Part 4 — Streaming top-K report (`top_k.hpp`)

`top_k_products(range, k)` over any single-pass input range, with a concept-constrained template. Returns the top `k` SKUs by units descending, SKU ascending on ties. Negative units throw.

- Memory scales with distinct SKUs plus `k`, not with the number of lines.

Concepts: `std::ranges::input_range` and `range_reference_t` constraints, forwarding references, a bounded min-heap with `priority_queue` and a custom comparator, single-pass guarantees, `views::iota | views::transform`.

## Part 5 — Layered configuration (`layered_config.hpp`)

A recursive `Value` (variant of null, bool, int64, string, list, table). Precedence is cli > env > file > defaults.

- `deep_merge`: tables merge key by key; lists and scalars are replaced; null overrides.
- `parse_env`: `APP_DB__POOL__SIZE` becomes a nested table, with bool and integer coercion and strict empty-segment rejection.
- `Config`: `get`, `source`, and `as_dict` over the layers; the layers are copied at construction.

Concepts: `std::variant` and recursive types through `std::map`/`std::vector` of incomplete types, `std::less<>` for heterogeneous lookup, deep copy versus shared state, `std::from_chars` for parsing, `optional` returns with `nullptr` as the not-found pointer.

## Part 6 — Inventory reconciliation (`reconcile.hpp`)

`reconcile(expected, counted)` produces `missing`, `unexpected`, and `mismatched` (counted minus expected). Zero-counted is a mismatch, not a missing SKU.

Concepts: sorted-range merge with `std::set_difference` or a manual two-pointer walk, `std::set` and `std::map` as result types, defaulted `operator==`.

## Part 7 — Volume pricing and SLA banding (`pricing.hpp`)

- `PriceTiers`: validated, sorted volume tiers. `unit_price` uses `upper_bound` (the boundary belongs to the new tier). Money is integer cents throughout.
- `sla_tier`: upper-inclusive bands (15, 60, 240 minutes); infinity maps to `low`; negative and NaN throw.

Concepts: `std::upper_bound` on a sorted vector, `std::map::lower_bound` for banding, validating constructors, exact integer money, floating-point edge cases.

## Build and run

From the repository root, on Windows prefix each command with `tools\vsenv.cmd`.

Exercises build (stubs; every test should FAIL):

```
tools\vsenv.cmd cmake --workflow --preset dev
tools\vsenv.cmd ctest --preset dev -R "01_stl_containers\.Part2"
tools\vsenv.cmd cmake --build --preset dev --target ex01_stl_containers_tests
```

Reference solutions:

```
tools\vsenv.cmd cmake --workflow --preset solutions
```

Run a single Part with `-R "01_stl_containers\.Part5"`.

## Stretch ideas (untested)

- Part 2: replace `std::function` with a template clock parameter to avoid type erasure; measure the difference.
- Part 2: add `std::pmr::monotonic_buffer_resource` for list nodes and compare allocation counts.
- Part 3: add a token-bucket variant and check burst behaviour against the sliding window.
- Part 4: replace the heap with a `std::flat_map` accumulator and compare memory against the current design.
- Part 5: add a `Value` JSON printer and round-trip it with a fixture file.
- Part 7: add currency-aware rounding for percentage discounts with `std::round` and documented half-even rules.
