# 16 · Unit testing a billing module

## Scenario

A payments team owns `billing/`: invoice totals, volume discounts, due dates, a
payment service that retries transient gateway failures and sends receipts, and
an atomic CSV export for the finance team. The code is provided and works. Your
job is the test suite that keeps it working: it is the thing that gets run on
every pull request, and the thing a reviewer trusts when someone touches the
rounding code at 5 pm on a Friday.

You write `student_suite.cpp` with GoogleTest and gMock. The grader does not
trust your suite; it checks it.

## Files

- `billing/billing.hpp`, `billing/billing.cpp`: provided. Do not edit.
  The grader compiles mutated copies of `billing.cpp` to see whether your suite notices.
- `student_suite.cpp`: yours. Every `TEST`, `TEST_F`, and `TEST_P` goes here.
  Split it into more `*_test.cpp` files if you like; the grader only needs
  the one `student_suite.cpp` to exist. Add a file to the test list in
  `CMakeLists.txt` if you do.
- `grader/run_suite.cmake`: the check that runs your suite (do not edit).

## Part 1 · Money and invoice totals

Money is integer cents; rates are basis points (10000 bps = 100%).

- `apply_bps(amount, bps)`: round half up to the nearest cent; reject negatives.
- `format_money`: `12.34`, `0.05`, `-2.50`.
- `LineItem` and `Invoice` validate their arguments (`InvalidInvoice`).
- `invoice_totals`: discount first, then tax on the discounted amount.

Test the rounding boundary, not just a happy-path value.

## Part 2 · Discount tiers

`discount_bps(subtotal)` is 0 below 10000, 200 from 10000, 500 from 50000, 1000
from 100000. Boundaries are inclusive. Use `TEST_P` with the boundary pairs,
not one hand-picked value per tier.

## Part 3 · Clock and dates

`Clock` is an injectable interface. Write a `FakeClock`; never read the real
system time in a test. Covered: UTC day extraction (23:30 UTC is still the
same day), due dates across leap days and year end, `net_days` validation,
and "overdue only strictly after the due day".

## Part 4 · Charging and retries

`PaymentService::charge` takes a `Gateway&`, `Notifier&`, `Logger&`, and a
`Sleeper&`. Use mocks:

- `StrictMock` for the gateway and sleeper. An unexpected call must fail the test.
- `NiceMock` only where you truly do not care about calls.
- `InSequence` for ordered retry behaviour.

Covered: success, zero total rejected before any gateway call, retry after
transient failure, retries exhausted (`PaymentFailed`, with the last
`TransientGatewayError` attached as a nested cause; read it with
`std::rethrow_if_nested`), declined payments never retried, unknown errors
propagate, backoff doubling capped at `max_delay`, constructor validation.

The suite must never really sleep. Inject the mock sleeper into every
`PaymentService` you build; the time budget check fails if it sleeps.

## Part 5 · Receipts and logging

- Success sends a receipt to the customer with the total and transaction id.
- A receipt failure is logged at error and does not fail the charge.
- Transient failures log at warning with the attempt count; declined payments log at error.

## Part 6 · CSV export

`export_invoices_csv` writes `<path>.tmp`, then renames it over `<path>`.
Covered: header and row format, tax after discount in the row, RFC 4180 quoting,
empty source, pulling from an `InvoiceSource` mock until it is exhausted,
replacing an existing file, and the failure path. On failure the previous file
must survive unchanged and no `.tmp` file may remain.

Use a per-test temporary directory under `std::filesystem::temp_directory_path()`
and remove it in `TearDown`.

## Grading

Your suite is graded by CMake and ctest:

- `16_unit_testing.Grader.baseline`: at least 30 tests are listed, and all of them pass against the real `billing.cpp`.
- `16_unit_testing.Grader.time_budget`: the whole suite finishes in under 10 seconds.
- `16_unit_testing.Grader.mutant_<name>`: about two dozen mutants of `billing.cpp`
  (off-by-one boundaries, rounding, tax order, timezone, backoff cap, retry
  counts, log levels, argument order, atomic-write cleanup, error isolation).
  Each mutant must make at least one of your tests fail. A mutant that
  passes is reported as a failure of its `Grader` test.

Every `TEST` you write is also a ctest on its own, named
`16_unit_testing.<Suite>.<Test>`.

## Concepts exercised

- GoogleTest: `TEST`, `TEST_F`, `TEST_P`, `INSTANTIATE_TEST_SUITE_P`, `EXPECT_` vs `ASSERT_`.
- gMock: `MOCK_METHOD`, `StrictMock`, `NiceMock`, `InSequence`, `Return`, `Throw`, matchers (`_`, `HasSubstr`, `AllOf`).
- Test seams: interfaces (`Clock`, `Gateway`, `Sleeper`) and fakes vs mocks.
- Exception testing: `EXPECT_THROW`, nested exceptions, `std::rethrow_if_nested`.
- Testing for atomicity and filesystem side effects with temporary directories.
- Boundary-value and property-style design: test the edge, not just the middle.

## Run

Windows, from the repo root (prefix every command with `tools\vsenv.cmd`):

    tools\vsenv.cmd cmake --workflow --preset dev
    tools\vsenv.cmd ctest --preset dev -R "16_unit_testing\.Part2"
    tools\vsenv.cmd cmake --build --preset dev --target ex16_unit_testing_tests
    tools\vsenv.cmd cmake --workflow --preset solutions

The workflow configures and builds the whole repo. Run only this exercise with
`-DCPPREF_ONLY=16_unit_testing`, as the other exercises do.

## Stretch ideas (not graded)

- Add a property-style test: for random subtotals, `discount + subtotal - discount == subtotal - discount` holds, and totals are monotonic in subtotal.
- Write a gMock matcher for `Totals` that prints a readable diff.
- Find a mutant the grader does not have yet and propose it.
