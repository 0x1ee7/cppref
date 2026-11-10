# cppref — modern C++ refresher, test-driven

Real-world C++20/23 exercises for an experienced engineer. The C++ sibling of
`pyref`: same philosophy (one topic per directory, a spec, failing tests, a
reference answer), designed around production C++ — RAII, value semantics,
move semantics, exception-safety guarantees, ranges, concepts, `std::expected`
— plus C++-specific topics (memory/RAII, templates, performance, build &
deployment).

## Setup

Requires CMake ≥ 3.28, Ninja and a C++23 compiler (MSVC 19.38+, GCC 14, Clang 18).
GoogleTest/gMock are fetched by CMake on first configure.

On Windows, run every command through `tools\vsenv.cmd`, which loads the
Visual Studio x64 developer environment (cl, link, and the CMake/Ninja that ship
with Visual Studio):

```powershell
tools\vsenv.cmd cmake --workflow --preset dev        # configure + build + test (all red at first)
```

On Linux/macOS drop the `tools\vsenv.cmd` prefix.

## Workflow

```powershell
tools\vsenv.cmd cmake --preset dev                                   # configure once
tools\vsenv.cmd cmake --build --preset dev --target ex12_logging_tests  # build one topic
tools\vsenv.cmd ctest --preset dev -R "12_logging"                   # one topic
tools\vsenv.cmd ctest --preset dev -R "12_logging\.Part2"            # one part (suites are Part<N>...)
tools\vsenv.cmd cmake --workflow --preset solutions                  # grade the reference answers
```

1. Read `exercises/<topic>/README.md` — it, together with the tests, is the spec.
2. Every `*.hpp` / `*.cpp` that is not `*_test.cpp` is yours. Headers declare the
   public API (C++ needs it to compile); definitions throw
   `std::logic_error("not implemented")`. Add private members, helpers and files
   freely — CMake globs the directory.
3. Iterate until green, then compare with `solutions/<topic>/`.

## How the harness works

`cmake/cppref.cmake` builds each topic as a library from your files plus a
GoogleTest executable from the `*_test.cpp` files; every TEST is its own ctest
(`<topic>.<Suite>.<Test>`), so one throwing stub fails only its own tests. The
`solutions` preset (`-DCPPREF_SOLUTIONS=ON`) takes the library sources from
`solutions/<topic>/` instead — same tests, reference code. `-DCPPREF_ONLY=<topic>`
configures a single topic.

With the stubs every test fails; with the `solutions` preset every test passes.

> `solutions/` is the answer key. Don't open it until your own version is green.

## Exercises

| # | Topic | What you build | Tests |
|---|---|---|---|
| 01 | [STL containers and value semantics](exercises/01_stl_containers) | Ingest filtering · TTL + LRU cache · Sliding-window rate limiter · Streaming top-K report · Layered configuration · Inventory reconciliation · Volume pricing and SLA banding | 57 |
| 02 | [Parameter passing](exercises/02_parameter_passing) | Sink by value, const&, && · Perfect forwarding and in-place construction · Defaults, options struct, and non-virtual interface · PATCH with `std::optional<std::optional<T>>` · string_view and span parameters, overload sets · Variadic parameters and keyword options for a query builder · Event bus: forwarding to listeners | 57 |
| 03 | [Namespaces and linkage](exercises/03_namespaces_and_linkage) | Inline namespaces, ADL and a JSON encoder · Internal and external linkage, inline definitions · Function-local statics · ADL, niebloids and name hiding · Lambda captures and lifetimes · RAII guards and self-registration · A formula evaluator | 63 |
| 04 | [Lambdas and callables](exercises/04_lambdas_and_callables) | Closures and composition · Forwarding decorator · Retry with backoff · Type erasure without ownership · Stateful callables · Overloads, variants, recursion, constexpr lambdas · Dispatch and middleware | 60 |
| 05 | [Iterators and Ranges](exercises/05_iterators_and_ranges) | Line Range · Pipeline · Paged Range · Generator · Batching · Moving Average · Merge Sorted | 64 |
| 06 | [Functional style in modern C++](exercises/06_functional) | Composition · Persistent list · Variant dispatch · Event sourcing · Exception-free parsing · Validator combinators · ETL pipeline | 77 |
| 07 | [Classes and Value Types](exercises/07_classes_and_value_types) | Currency and Money construction, formatting, hashing · Money arithmetic, comparison, rounding, allocation · Product (validated value type with class-level state) · Account hierarchy (non-virtual interface with protected hooks) · ShoppingCart (value aggregate with a stable iteration order) · Employee (factory from aggregate and from CSV row) · Notifications (abstract base, concrete channels, dispatcher) · Version (semantic versioning value type) | 74 |
| 08 | [Polymorphism and type erasure](exercises/08_polymorphism_and_type_erasure) | Interfaces and the Non-Virtual Interface idiom · Ownership and the registry · CRTP mixins · Typed fields and model validation · Type erasure: Task and PropertyBag · Visitors and variants · Lazy proxy and the virtual-base diamond | 68 |
| 09 | [RAII and memory management](exercises/09_raii_and_memory) | Handles · Rule of five · Unique ownership with a stateful deleter · Shared ownership and weak caching · Scope guards and temporary directories · Transactions · Pool leases and arena allocation | 103 |
| 10 | [Error handling in C++](exercises/10_error_handling) | Exception hierarchy · Chaining causes · Exception-safety guarantees · noexcept and containers · std::error_code · std::expected · Batches and retries | 63 |
| 11 | [Templates and Concepts](exercises/11_templates_and_concepts) | Concepts and subsumption · Constrained generic repository · Variadic templates, fold expressions, if constexpr · Strong typedefs · Units of measure · Compile-time strings, routes and CTAD · Type-keyed event bus | 58 |
| 12 | [Logging library](exercises/12_logging) | Levels and hierarchy · Lazy formatting, call sites, dispatch · Formatters and sinks · Asynchronous sink and concurrency · Mapped diagnostic context (MDC) · Redaction | 77 |
| 13 | [Files & Serialization](exercises/13_files_and_serialization) | Hashing and checksums · Tree scan and ignore globs · Diff, key validation, and apply · Minimal JSON and manifest documents · Atomic writes and manifest persistence · RFC 4180 CSV report · Versioned big-endian snapshot header | 102 |
| 14 | [SQLite-backed order management](exercises/14_database_sqlite) | Database and statements · Binding and reading · Transactions and savepoints · Schema migrations · Repositories · Bulk import · Order queries · Reports | 68 |
| 15 | [Concurrency](exercises/15_concurrency) | Concurrent link checker · Bounded downloader · Shared state · Caches · Bounded queue, multi-stage pipeline · Thread pool and futures · Lock-free SPSC ring and periodic scheduler | 76 |
| 16 | [Unit testing a billing module](exercises/16_unit_testing) | Money and invoice totals · Discount tiers · Clock and dates · Charging and retries · Receipts and logging · CSV export | 75 |
| 17 | [`<chrono>`: durations, calendars, time zones](exercises/17_chrono_time) | durations, rounding, epoch milliseconds · parsing · zones and DST-aware arithmetic · calendars, billing dates, business days · recurring meetings with DST policies · cross-zone slots · humanize · Stopwatch over an injected steady clock | 71 |
| 18 | [Build and deployment](exercises/18_build_and_deployment) | Configuration from the environment · The command-line front end · Build metadata · Graceful shutdown · Readiness checks · Container files | 74 |
| 19 | [Performance: allocation budgets and data layout](exercises/19_performance) | Zero-allocation log parsing · Single-allocation string building · Inline-storage containers · Move semantics and reallocation policy · Request-scoped parsing with std::pmr · Layout and columnar aggregation | 99 |

Topic notes:

- **16 Unit testing** is inverted: you write `student_suite.cpp` against the provided
  `billing/` module. CMake generates 24 mutants of `billing.cpp` at configure time and
  registers each as a `WILL_FAIL` ctest, plus `Grader.baseline` and `Grader.time_budget`.
- **14 SQLite** fetches the pinned SQLite amalgamation (3.47.2) via FetchContent.
- **17 Chrono** needs a tzdb: MSVC and libstdc++ work; Clang's libc++ may not.
- **18 Build & deployment** — you also own `Dockerfile` and `.dockerignore`; two tests
  run the built `app` binary.
- **19 Performance** replaces global `operator new` in the test binary to enforce
  allocation budgets.
- Only MSVC 19.38 was used to verify; the code targets standard C++23 for GCC 14 / Clang 18.

