# 11 — Templates and Concepts

Your service's shared building blocks are generic on purpose. One ID type
must not be mixed with another. The repository works for any entity that has
an ordered id. Units stop metres being added to seconds. The event bus routes
events by type. The route checker rejects a malformed pattern before the
binary is built. All of it is compile-time checked, with no runtime cost
where the design promises none.

You implement the header and source stubs in this directory. Each Part's
tests in `part*_test.cpp` are the specification. Every test currently fails
with `not implemented: ...` until you fill in the stubs.

## Part 1 — Concepts and subsumption

`concepts.hpp`. Define `Entity<T>` (has a `T::id_type` that is totally
ordered, and a const `id()` returning exactly that type) and `Named<T>`
(const `name()` convertible to `std::string_view`). Implement three
`describe` overloads: unconstrained returns `"value"`; `requires Entity<T>`
returns `"entity:<id>"`; `requires Entity<T> && Named<T>` returns
`"entity:<id>:<name>"`. The most constrained viable overload must win through
subsumption, not through `if constexpr`.

`total_of(range)` sums any input range whose elements convert to `long long`,
for lvalues and views alike. A range of non-convertible elements must be
rejected at the call site by the constraint.

- Concepts: `requires`-clauses, atomic vs. conjunction constraints,
  `std::ranges::input_range`, `range_reference_t`, subsumption.

## Part 2 — Constrained generic repository

`repository.hpp`. `Repository<T>` requires `Entity<T>`. `add` rejects a
duplicate id and keeps the original. `find` returns a copy or `std::nullopt`.
`remove` reports whether something was removed. `all()` and `where(pred)`
iterate in ascending id order, not insertion order. `size()` and `empty()` are
`noexcept`. `Repository<int>` must not be well-formed.

- Concepts: constrained class templates, `std::predicate`, a container choice
  driven by ordering requirements, `noexcept` on queries.

## Part 3 — Variadic templates, fold expressions, `if constexpr`

`variadic.hpp`.
- `sum_all(Ts...)`: integral arguments, at least one; result is the common
  type of the arguments.
- `count_of_v<T, Ts...>`: count of `T` in the pack, computed by a fold.
- `to_text`, `join_text(sep, parts...)`: `bool` renders `true`/`false`;
  string-like values pass through; other arithmetic values use `to_string`.
  An empty pack gives `""`. Non-text types are rejected by `TextValue`.
- `kind_name<T>()`: `"boolean"`, `"integer"`, `"floating"`, `"text"`,
  `"other"`, chosen with `if constexpr` and type traits.
- `parse_or<T>(text, fallback)`: integral and floating types must consume the
  whole text (use `std::from_chars`); `bool` accepts exactly `"true"` or
  `"false"`; `std::string` always succeeds. `std::type_identity_t` makes the
  fallback non-deduced.

- Concepts: variadic packs, fold expressions (unary and binary), `if
  constexpr`, `std::common_type_t`, `std::type_identity_t`,
  `std::from_chars`.

## Part 4 — Strong typedefs

`strong_id.hpp`. `StrongId<Tag, Rep>` wraps an unsigned integer with a phantom
tag. Construction is explicit. Equality and ordering exist only between the
same StrongId type. `std::hash` is specialized so the type can be an
`unordered_map` key. Layout matches `Rep`: `sizeof(UserId) == 8`, trivially
copyable. Ids of different tags must not compare.

- Concepts: phantom types, explicit conversions, defaulted `==` and `<=>`,
  hidden friends, `std::hash` specialization, zero-cost wrapping.

## Part 5 — Units of measure

`units.hpp`. `Quantity<Dim<L, T>>` holds a `double`, with construction
explicit. Addition and subtraction exist only within one dimension. Product
and quotient combine dimension exponents: `meters * meters` gives
`SquareMeters`, `meters / seconds` gives `MetersPerSecond`,
`meters / meters` is dimensionless. Scaling by a `double` keeps the dimension.
`CanAdd<A, B>` is true only when the two quantities can be added.

- Concepts: non-type template parameters on a struct (`Dim<int, int>`),
  dimension arithmetic through alias templates, hidden friends, `requires`
  expressions.

## Part 6 — Compile-time strings, routes and CTAD

`compile_time.hpp`.
- `FixedString<N>`: a class-type NTTP holding a string literal. `view()`
  excludes the NUL.
- `route_is_valid(pattern)`: `constexpr` check. Leading `/`; no empty segments;
  `{name}` is a whole segment with a non-empty name; nothing after the closing
  brace in that segment; no stray braces. `"/"` alone is valid.
- `checked_param_count(pattern)`: `consteval`; an invalid pattern must not
  compile.
- `Route<"/users/{id}">`: `pattern`, `is_valid`, `param_count` as
  compile-time values, and `matches(path)` at runtime. A `{name}` segment
  matches any non-empty segment; segment counts must agree.
- `Labeled<T>` with a deduction guide: `Labeled{"hi"}` is
  `Labeled<std::string>`, `Labeled{42}` is `Labeled<int>`.

- Concepts: class-type NTTPs, `consteval` vs. `constexpr`, CTAD and
  deduction guides, compile-time validation through constant evaluation.

## Part 7 — Type-keyed event bus

`event_bus.hpp`, `event_bus.cpp`. `EventBus` routes events by their type.
`subscribe<E>(handler)` returns a `[[nodiscard]] Subscription`, an RAII
handle that cancels on destruction or on `cancel()`. Both cancel paths are
idempotent. `publish<E>(event)` returns how many handlers ran.
`subscriber_count<E>()` counts live registrations. Dispatch semantics:
- Publish snapshots live registrations first. A handler subscribed during
  dispatch does not see the current event.
- A handler cancelled during dispatch is skipped.
- A throwing handler propagates the exception, stops dispatch for that event,
  and leaves the bus usable.
- The same callable subscribed twice is two registrations.
- A `Subscription` may outlive its bus; cancelling it is then a safe no-op.
- The bus is move-only.

`EventHandler<H, E>` is a concept: `std::invocable<H, const E&>`.

- Concepts: type-erased storage behind a template front-end, `std::type_index`,
  `std::function`, RAII with move-only handles, `weak_ptr` for handles that
  outlive their owner, `[[nodiscard]]`.

## Concepts exercised

- Requires-clauses, concept definitions, subsumption, `std::invocable`,
  `std::ranges`
- Variadic templates, fold expressions, `if constexpr`, `std::type_identity_t`,
  `std::common_type_t`, `std::from_chars`
- Phantom types, explicit conversions, defaulted comparisons, `std::hash`
  specialization
- Dimensional analysis with non-type template parameters and alias templates
- Class-type NTTPs, `consteval`, CTAD and deduction guides
- Type-erased dispatch behind a template front-end, RAII handles, weak links

## Run

From the repository root, on Windows prefix each command with `tools\vsenv.cmd`.

```
cmake --workflow --preset dev
ctest --preset dev -R "11_templates_and_concepts\\.Part2"
cmake --build --preset dev --target ex11_templates_and_concepts_tests
```

Reference solutions (run after you finish, to check your work):

```
cmake --workflow --preset solutions
```

## Stretch (untested)

- Add a `Repository` overload that accepts a custom comparator for the
  ordered container, and constrain it with `std::strict_weak_order`.
- Give `Quantity` a `std::formatter` specialization that prints the unit
  suffix from the dimension.
- Replace `Route::matches` with a compile-time matcher that returns a tuple of
  `std::string_view` captures.
- Extend `EventBus` with a priority parameter; keep the snapshot semantics.
