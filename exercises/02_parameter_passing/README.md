# 02 · Parameter passing

**Scenario.** You are building the client-side plumbing of an internal service SDK: a request
builder, a PATCH endpoint for user profiles, a log formatter, a SQL query builder for a reporting
tool, and an in-process event bus. Every function here takes arguments, and every argument has a
price: a copy, a move, an allocation, or a dangling view. The point of this exercise is to pick the
right passing mode for each parameter and to prove the cost with an instrumented type.

The Python sibling (`pyref/exercises/02_function_arguments`) covers defaults, keyword arguments,
sentinels, and `*args`/`**kwargs`. Here those ideas map to sink-by-value, perfect forwarding,
designated initializers, `std::optional<std::optional<T>>`, `std::string_view`/`std::span`
parameters, variadic templates, and overload sets.

Files you own: `parameter_passing.hpp` and `parameter_passing.cpp`. The tests in
`parameter_passing_test.cpp` are the specification; `Payload` in that file counts copies and moves.

## Part 1 · Sink by value, `const&`, `&&`

`Slot<T>` stores one `T`. Its API documents the cost of each entry point:

- `explicit Slot(T value)` and `reset(T value)` are sinks. An lvalue costs one copy plus one move
  (copy into the parameter, move into the member). A named rvalue costs two moves. A prvalue costs
  one move.
- `assign(const T&)` copies only. `assign(T&&)` moves only.
- `get()` returns `const T&` and costs nothing.
- `std::move(slot).take()` moves the value out.

**Concepts:** pass-by-value sink idiom, value categories, copy/move elision for prvalues, rvalue
reference overloads, ref-qualified member functions (`&&`).

## Part 2 · Perfect forwarding and in-place construction

- `make_owned<T>(args...)` returns `std::unique_ptr<T>` and forwards each argument with its value
  category. An lvalue `Payload` is copied, an rvalue is moved.
- `Registry<T>::emplace(key, args...)` builds the value in the map node from the forwarded arguments.
  Duplicate keys throw `std::invalid_argument`. Because `try_emplace` is used, the arguments are not
  consumed on that path.
- `find(key)` takes `std::string_view` and uses a transparent comparator.

**Concepts:** `Args&&...` with `std::forward`, `std::constructible_from` constraints, `try_emplace`
versus `emplace`, transparent lookup (`std::less<>`), the non-consumption guarantee.

## Part 3 · Defaults, options struct, and non-virtual interface

- `parse_method` is case-insensitive. Unknown text throws. `method_name(Method::del)` is `"DELETE"`.
- `build_request(method, url, RequestOptions options = {})` takes its options by value (a sink).
  Header names are lowercased and their order is preserved. The URL must start with `http://` or
  `https://` (case-insensitive). `retries < 0` throws.
- `RequestOptions` is an options struct with defaults. Call sites use designated initializers, so
  only the fields you name change.
- `Transport` has a non-virtual `send(request, attempts = 3) const`. It validates `attempts >= 1`
  and then calls the private pure virtual `do_send`. The default argument is therefore the same for
  every dynamic type, because default arguments bind statically.

**Concepts:** designated initializers as named arguments, default arguments and static binding,
non-virtual interface (NVI), `explicit` and deleted copy on polymorphic bases, argument validation
before the backend is called.

## Part 4 · PATCH with `std::optional<std::optional<T>>`

`PatchField` distinguishes three states:

| Outer | Inner | Meaning |
|---|---|---|
| `nullopt` | (n/a) | absent: leave the field alone |
| engaged | `nullopt` | clear the field |
| engaged | value | set the field |

- `apply_patch(User user, const UserPatch&)` takes the user by value and returns the patched copy.
  The caller's object is never modified.
- Email: trimmed and lowercased. It must match `[^@\s]+@[^@\s]+\.[^@\s]+`. Clearing it throws.
- Nickname: trimmed. Empty or longer than 32 characters throws. Clearing sets `nullopt`.

**Concepts:** nested optionals as a tri-state, by-value parameter returned as the result (copy
elision), strong exception guarantee for a by-value update.

## Part 5 · `string_view` and `span` parameters, overload sets

- `split_fields(line, sep)` returns `std::string_view`s that alias `line`. Empty fields are kept:
  `""` gives one empty field.
- `join_fields(std::span<const std::string_view>, sep)` reads the span and never retains it.
- `format_value` overloads: `bool`, `std::int64_t`, `double`, `std::string_view`, and `const char*`.
  Strings are quoted. A `const char*` is routed to its own overload so that it never decays to
  `bool`. A plain `int` argument is ambiguous by design; tests cast it.

**Concepts:** non-owning views and their lifetime, `std::span` as the universal contiguous
parameter, overload resolution (exact match, array-to-pointer, user-defined conversions), the
`bool` trap for `const char*`.

## Part 6 · Variadic parameters and keyword options for a query builder

`select(table, options, columns...)` is a variadic template that accepts any column type
convertible to `std::string_view`. It produces an `Sql { text; params; }`.

- Values are only ever placeholders (`?`). `params` holds `Value = variant<nullptr_t, bool,
  int64_t, string>`.
- `SelectOptions` is a keyword-style struct:
  - `where`: a `std::map<std::string, Value, std::less<>>`, rendered as `col = ?` joined by `AND`
    in key order. A `nullptr` value renders `col IS NULL`.
  - `order_by`: `"col"` is ascending, `"-col"` is `DESC`.
  - `limit`: an optional `LIMIT ?`. A negative value throws.
- Every identifier (table, columns, where keys, order terms) must match `[A-Za-z_][A-Za-z0-9_]*`
  with at most one dotted part. Anything else throws.

Expected rendering:
`SELECT id, email FROM users WHERE active = ? AND deleted_at IS NULL ORDER BY id DESC LIMIT ?`
with params `{true, 20}`.

**Concepts:** variadic function templates, constraints on pack elements (`requires (... && ...)`),
the "thin template, fat non-template" split (`detail::select_impl`), designated initializers for
options, `std::variant` as a value type, SQL-injection-proof API design.

## Part 7 · Event bus: forwarding to listeners

`EventBus<Args...>` is typed by its event payload.

- `subscribe(event, listener, SubscribeOptions{.once})` returns a `SubscriptionId`. An empty
  listener throws.
- `unsubscribe(id)` returns `true` once and `false` afterwards.
- `publish(event, args...)` takes its arguments by value (one sink per publish). Listeners are
  `std::function<void(const Args&...)>`, so they borrow the payload and never copy it.
  - Exact cost: a `const&` listener with an rvalue publish costs one move. An lvalue publish costs
    one copy. A by-value listener costs an extra copy per call.
- `publish` iterates over a snapshot. A listener removed mid-publish still runs for that publish.
  A `once` listener is removed before it runs, so it never runs twice, even on a nested publish.
  A throwing listener stops the publish and the exception propagates.

**Concepts:** forwarding a payload once and borrowing it, `std::function` storage and its copy cost,
snapshot iteration under mutation, reentrancy, `once` semantics, exception propagation.

## Run

From the repository root on Windows, prefix every command with `tools\vsenv.cmd`:

```
tools\vsenv.cmd cmake --workflow --preset dev
tools\vsenv.cmd ctest --preset dev -R "02_parameter_passing\.Part2"
tools\vsenv.cmd cmake --build --preset dev --target ex02_parameter_passing_tests
```

Reference solutions:

```
tools\vsenv.cmd cmake --workflow --preset solutions
```

Every test fails until you implement the stubs. Each Part is a separate ctest suite, so run one
Part at a time.

## Stretch (not tested)

- Make `Slot<T>` conditionally `noexcept` on the move operations of `T`.
- Add a `std::move_only_function` variant of the event bus listener and measure the difference.
- Replace `format_value(double)` with `std::to_chars` and compare the output.
- Add an `in_place` overload to `Slot` and prove it costs one construction and no moves.
