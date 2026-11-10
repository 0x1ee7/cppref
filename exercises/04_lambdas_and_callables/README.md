# 04 · Lambdas and callables

## Scenario

You maintain a small service toolkit. Its pieces are glue code: a timing decorator around handlers, a retry wrapper around flaky RPC clients, non-owning callbacks passed into hot loops, a rate limiter and memo cache in front of expensive lookups, an event dispatcher, and an HTTP-style middleware chain. Every piece is a callable, and each one has a different ownership and lifetime story. Get the lifetimes wrong and you get dangling references or silent copies. Get the signatures wrong and you get surprising overloads or template errors.

Build the toolkit in `cppref::lambdas`. The header is the contract: keep names and signatures.

## Parts

### Part 1 · Closures and composition
- `make_multiplier(factor)`: captures `factor` by value.
- `make_accumulator()`: a mutable closure with a private total. Each call gets an independent total.
- `make_adders(n)`: `n` callables, where element `i` computes `x + i`. Throws `std::invalid_argument` if `n < 0`.
- `compose(f1, ..., fn)(x) == f1(f2(...fn(x)))`. `compose()` is the identity. Stored callables are invoked through const references.

Edge cases: capture by value vs by reference, `mutable` closures, the empty pack, and a copy of an accumulator that keeps its state.

### Part 2 · Forwarding decorator
- `Timed<Fn, Clock>` holds `Fn` by value and forwards arguments perfectly. It returns exactly what `Fn` returns: `void`, references, and move-only values all survive.
- Each call reads the clock before and after, and reports one `Timing` to the sink. This includes when `Fn` throws (`threw = true`), and the exception is rethrown.
- `timed(label, fn, sink, clock)` deduces the types.

Edge cases: void vs non-void returns, move-only return values, an exception path that still reports, a clock injected for determinism.

### Part 3 · Retry with backoff
- `retry(fn, policy, sleep)` returns a `Retrying` callable. `fn` returns `std::expected<T, std::error_code>`.
- Delay before retry `k` (1-based) is `min(base_delay * multiplier^(k-1), max_delay)`. No sleep after the last attempt.
- Stop early when the result succeeds or `retryable(error)` is false. Return the last result.
- `validate_policy` throws `std::invalid_argument` for `max_attempts < 1`, `base_delay < 0`, `max_delay < base_delay`, or `multiplier < 1.0`.
- Arguments are passed to `fn` as lvalues on every attempt.

Edge cases: first-try success, non-retryable errors, exhaustion, cap at `max_delay`, and policy validation.

### Part 4 · Type erasure without ownership
- `FunctionRef<R(Args...)>` is a non-owning reference: two pointers, and only lvalues bind. A temporary lambda does not compile, which prevents a dangling reference.
- `sum_with(xs, f)` sums `f(x)` over a span.
- `tag_line(tag, body)` formats `"[tag] body"`. `make_tagger(tag)` returns an owning `std::function` that captures its tag by value.

Edge cases: size of `FunctionRef`, lvalue-only binding, `const` callables, and ownership vs reference semantics.

### Part 5 · Stateful callables
- `CallCounter<F>` counts calls. Copies carry their count. `reset()` zeroes it.
- `SlidingWindowLimiter(max_calls, period, now)`: at most `max_calls` acquisitions in any window of length `period`. An acquisition at time `t` stops counting once `t + period <= now`. When the window is full, `try_acquire` returns the time until the oldest expires. The constructor throws for `max_calls == 0` or `period <= 0`.
- `RateLimited<F>`: refused calls do not invoke `F`. Admitted calls return `std::expected` with `F`'s result. Refused calls return the retry-after duration.
- `MemoizedTtl<R(Args...)>` / `memoize_ttl<Sig>(fn, ttl, now)`: keys are `std::tuple` of decayed arguments. An entry is a hit while `now < stored + ttl`. `clear()` drops entries but keeps the hit and miss counters.

Edge cases: window boundaries (`t + period == now` is expired), retry-after math, ttl boundaries, `void` callables under `RateLimited`, and counters that survive `clear()`.

### Part 6 · Overloads, variants, recursion, constexpr lambdas
- `overloaded{...}` builds a visitor from lambdas. The class must inherit all call operators with `using Ts::operator()...;`.
- `describe(Event)` visits a `std::variant` of `Connected`, `Message`, and `Disconnected` into a string.
- `count_nodes` is a deducing-this recursive lambda over a rose tree. `render_tree` writes one line per node, indented two spaces per depth.
- `is_valid_port` is a constexpr lambda, usable in constant expressions.

Edge cases: overload resolution, visiting every alternative, recursion without a named helper, and constant evaluation.

### Part 7 · Dispatch and middleware
- `CommandRegistry`: `add` throws `std::invalid_argument` for an empty or duplicate name. `run` throws `UnknownCommand` (derived from `std::out_of_range`) for an unknown name. `names()` returns sorted names. Lookup is heterogeneous: `string_view` keys work.
- `Handler` maps `Request` to `Response`. `Middleware` maps `Handler` to `Handler`.
- `build_chain(terminal, middleware)`: the first element is the outermost wrapper.
- `recover_errors()`: an exception from the inner handler becomes a 500 with `e.what()` as the body.
- `require_header(name)`: responds 400 `"missing header: <name>"` without calling the inner handler.
- `access_log(sink)`: after each response, calls the sink with `"<METHOD> <path> <status>"`.

Edge cases: chain ordering, short-circuiting, exception containment, and copies of closures held in `std::function`.

## Concepts exercised

- Lambda captures (by value, by reference, init-capture), `mutable`, and closure copy semantics.
- `std::function` as an owning type-erased callable, and its cost.
- Perfect forwarding, `std::invoke`, `std::invoke_result_t`, and preserving value category and `void` returns.
- Non-owning callable references (`FunctionRef`) and the dangling-temporary trap.
- Stateful functors, const-correctness of `operator()`, and `std::expected` for recoverable errors.
- Injected clocks (`NowFn`) for deterministic time-based tests.
- Overloaded visitors (`overloaded`, `std::visit`), deducing-this recursion, and constexpr lambdas.
- Middleware composition and ordering of wrappers.

## Run
Prefix each command with `tools\vsenv.cmd`:

```
tools\vsenv.cmd cmake --workflow --preset dev
tools\vsenv.cmd ctest --preset dev -R "04_lambdas_and_callables\.Part2"
tools\vsenv.cmd cmake --build --preset dev --target ex04_lambdas_and_callables_tests
tools\vsenv.cmd cmake --workflow --preset solutions
```

Before you implement anything, every test fails. The stubs throw `std::logic_error("not implemented: ...")`.

## Stretch (untested)

- Make `Timed` report to a lock-free ring buffer and measure the overhead against a bare call.
- Add `std::move_only_function` (C++23) to `Timed` and `Retrying` and test move-only callables.
- Build `FunctionRef` over a member function pointer and bind an object plus a method.
- Add jitter to `Retrying` using an injected RNG, then test it deterministically.
- Let `CommandRegistry` accept a coroutine-based command (out of scope for this exercise).
