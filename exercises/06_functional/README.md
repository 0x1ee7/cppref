# 06 · Functional style in modern C++

A senior-level tour of functional building blocks in C++23: callable composition,
persistent (immutable) data, `std::variant` dispatch, `std::expected` error
pipelines, validator combinators, and a small ETL report built from those pieces.

Every part is a real service-side concern: request validation, config loading,
cart and account state machines, and report generation.

## Part 1 · Composition (`composition.hpp`)

`compose(f, g, h)(x)` is `f(g(h(x)))`. `pipe(v, f, g)` threads a value left to
right and may change type at each step. `partial_right(f, b...)` binds trailing
arguments. Callables are stored by value, so they can outlive the expression that
created them.

Concepts exercised: variadic forwarding, decayed storage, `std::invoke`,
type-changing pipelines, `auto` return deduction.

## Part 2 · Persistent list (`persistent_list.hpp`)

An immutable singly linked list with structural sharing. `cons`, `pop_front`, and
`set` return new lists and share the unchanged suffix. `shares_node_at` checks
identity, not equality. `map` can change the element type.

Concepts exercised: `shared_ptr<const Node>`, copy-on-write prefix rebuild,
`[[nodiscard]]`, a non-recursive destructor for 200,000-node lists.

## Part 3 · Variant dispatch (`variant_utils.hpp`, `cart.hpp`, `cart.cpp`)

`overloaded{...}` builds a `std::visit` visitor from lambdas. A shopping cart is a
pure reducer: `reduce_cart(state, action)` returns a new state or a `CartError`,
and `run_actions` stops at the first failing action.

Concepts exercised: `std::variant` of action structs, CTAD for aggregates with
base classes, `std::visit` exhaustiveness, value-semantic state, floor division for
money.

## Part 4 · Event sourcing (`account.hpp`, `account.cpp`)

An account is rebuilt by folding events with `apply_event(optional<Account>,
event)`. `replay` returns the final state, and `snapshots` keeps every intermediate
state. `decide_withdraw` is a pure decision function that runs before the event is
appended.

Concepts exercised: `std::optional` as an "absent aggregate", `std::expected`
error propagation, the fixed check order (existence, amount, frozen, funds).

## Part 5 · Exception-free parsing (`expected_utils.hpp`, `config.hpp`, `config.cpp`)

`try_call` converts a thrown exception into `std::exception_ptr` inside an
`expected`. `collect` turns a range of `expected` values into one
`expected<vector>` and stops at the first error. `parse_config` is the opposite
case: it accumulates every error with its line number.

Concepts exercised: `std::invoke_result_t`, `std::exception_ptr`, input ranges
with projections, the difference between fail-fast and accumulate-all.

## Part 6 · Validator combinators (`validation.hpp`, `validation.cpp`)

A validator is a callable `Value -> Errors`. Primitives (`is_type`, `min_length`,
`matches`, `in_range`) combine through `all_of`, `any_of`, `field`, `optional`,
and `each`. `validate_signup` is built from them.

Concepts exercised: `std::function` as a type-erased combinator, `std::variant`
for a dynamic JSON-like value, short-circuit `any_of`, `std::format` messages,
`std::initializer_list` parameters.

## Part 7 · ETL pipeline (`etl.hpp`, `etl.cpp`)

Raw string rows become `Sale` records through `parse_cents` (exact money, no
floating point), `parse_date` (calendar-checked), and `parse_sale`. `clean_sales`
filters first. `totals_by` takes a projection, `ranked` sorts with ranges, and
`build_report` chains the steps with `pipe()`.

Concepts exercised: `std::chrono::year_month_day`, `std::ranges::sort` with a
comparator, template member-pointer projections, `std::format` grouping, pipelines
over `expected::transform`.

## Build and run

Prefix each command with `tools\vsenv.cmd`:

```
tools\vsenv.cmd cmake --workflow --preset dev
tools\vsenv.cmd ctest --preset dev -R "06_functional\.Part2"
tools\vsenv.cmd cmake --build --preset dev --target ex06_functional_tests
tools\vsenv.cmd cmake --workflow --preset solutions
```

Parts 1–7 are each a separate suite (`Part1Composition` … `Part7Etl`).

## Stretch ideas (untested)

- Add `zip_with` and `scan` over `PersistentList` with lazy ranges.
- Make `Composed` `constexpr` and check it with `static_assert`.
- Replace `std::function` in the validators with a template combinator and compare
  binary size and compile time.
- Add a `retry` combinator with an exponential backoff policy.
