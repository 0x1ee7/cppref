# 10 · Error handling in C++

## Scenario

You own the configuration and payment-ingest layer of a small order service.
It loads an INI-style config at startup, applies inventory deltas in batches,
talks to a payment gateway that returns HTTP-style failures, parses order
lines from a CSV feed, and runs nightly batch jobs that must survive
individual task failures and retry transient ones.

Every layer has to choose a failure model deliberately: exceptions where a
failure is exceptional, `std::expected` where it is a normal outcome, and
`std::error_code` where a stable, comparable code matters across an ABI or
library boundary. The exercise walks through each of these and the guarantees
they make.

## Layout

| File | Owned by you | Purpose |
|---|---|---|
| `errors.hpp/.cpp` | yes | Exception hierarchy base, `describe_chain` |
| `config.hpp/.cpp` | yes | Parser, loader, typed getters, expected boundary |
| `inventory.hpp` | yes | `Inventory<Entry>` with strong and basic guarantees |
| `probe.hpp` | yes | Copy/move observer for `noexcept` analysis |
| `gateway.hpp/.cpp` | yes | `std::error_code` enum and category |
| `amounts.hpp/.cpp` | yes | Money parsing with `std::expected` |
| `batch.hpp/.cpp` | yes | Batch runner, retry policy, triage |

Stubs throw `std::logic_error("not implemented: <name>")`, so every test
fails until you implement its function.

## Parts

### Part 1 · Exception hierarchy
`DomainError : std::runtime_error` is the base for this layer. `ConfigError`
derives from it, with three leaves: `ConfigNotFound`, `ConfigUnreadable`, and
`ConfigInvalid`. `SettingMissing(key)` is a `ConfigError` that exposes the
missing `key()`. Tests check the inheritance tree, `catch` by base class, and
that `what()` carries the context the caller needs.

### Part 2 · Chaining causes
`load_config` maps low-level failures to domain errors and keeps the original
error via `std::throw_with_nested`. A missing file becomes `ConfigNotFound`
over a `std::system_error`. A malformed file becomes `ConfigInvalid` over a
`ParseError` (with `line()`). `describe_chain` walks the chain with
`std::nested_exception::nested_ptr()` and rethrows each link to read its
type. Non-`std` links are reported as `"unknown exception"`.

### Part 3 · Exception-safety guarantees
`Inventory<Entry>::apply` provides the **strong guarantee**: copy the ledger
into a new vector, add the batch, then `swap`. If a copy throws, the ledger
is unchanged. `apply_in_place` provides only the **basic guarantee**: it keeps
the entries applied so far. Tests inject throwing copy constructors (via
`TEST_P`, with a copy budget) to check each guarantee exactly.

### Part 4 · `noexcept` and containers
`Probe<NothrowMove>` counts copies and moves. `std::vector` moves elements on
growth only when the move constructor is `noexcept`; otherwise it copies (for
copyable types). The stub omits `noexcept(NothrowMove)`, which is the
deliberate gap. Tests verify which operation the vector used during growth and
that `std::move_if_noexcept` behaves the same way.

### Part 5 · `std::error_code`
`GatewayErrc` is an enum that opts into `std::is_error_code_enum`. Its
category `gateway_category()` (named `"payment-gateway"`) maps each code to a
message and to a portable `std::errc` condition, so callers can write
`ec == std::errc::timed_out`. `from_http_status` translates an HTTP status
into a code, and `throw_gateway_error` throws `std::system_error` for callers
that prefer exceptions.

### Part 6 · `std::expected`
`parse_amount`, `parse_quantity`, and `parse_line_item` return
`std::expected` and never throw on bad input. `parse_line_item` composes its
fields with `transform_error`, `and_then`, and `transform`, tagging each
failure with the field index. Money is stored as integer cents and is never a
`double`. `try_load_config` and `value_or_throw` convert in both directions
between the exception and `expected` styles.

### Part 7 · Batches and retries
`run_batch` runs every task and collects failures into a `BatchError`, the
analogue of `ExceptionGroup`. Each failure keeps its `std::exception_ptr`, so
its dynamic type survives. `is_transient` classifies failures by
`std::errc`. `retry_call` retries transient failures with exponential backoff
(sleep is injected through `RetryPolicy`), rethrows permanent failures at
once, and throws `RetryExhausted` with the last error nested. `triage` splits
a batch's failures by classification.

## Concepts exercised

- Exception hierarchies, catching by base, and `what()` messages
- `std::throw_with_nested`, `std::nested_exception`, `std::rethrow_exception`
- `std::exception_ptr` as a value: capturing, storing, rethrowing
- Strong vs basic exception-safety guarantees, copy-and-swap
- `noexcept` as a contract, `std::move_if_noexcept`, container growth
- `std::error_code`, `std::error_category`, `is_error_code_enum`, portable conditions
- `std::expected`, `transform`, `transform_error`, `and_then`, error-to-exception bridging
- Retry policy design with injectable sleep and classification

## Run

Windows: prefix each command with `tools\vsenv.cmd` from the repository root.

```
tools\vsenv.cmd cmake --workflow --preset dev
tools\vsenv.cmd ctest --preset dev -R "10_error_handling\.Part2"
tools\vsenv.cmd cmake --build --preset dev --target ex10_error_handling_tests
tools\vsenv.cmd cmake --workflow --preset solutions
```

Each Part maps to a test suite (`Part1Hierarchy`, `Part2Chaining`,
`Part3Guarantees`, `Part3StrongApply`, `Part4Noexcept`, `Part5ErrorCode`,
`Part6Expected`, `Part7Batch`, `Part7Retry`). Run one Part with
`-R "10_error_handling\.Part3"`.

## Stretch (not tested)

- Replace `RetryPolicy::sleep` with a `std::stop_token`-aware wait, so a
  shutdown interrupts backoff.
- Add `std::error_code` support to `parse_config` and compare its cost with
  the exception path under a fast-fail benchmark.
- Port `describe_chain` to print the `std::stacktrace` of each link (C++23;
  check availability on your toolchain).
- Write a `ConfigFailure`-to-`std::errc` mapping and decide whether it belongs
  to the public API.
