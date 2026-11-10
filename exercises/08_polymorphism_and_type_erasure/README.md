# 08 - Polymorphism and type erasure

## Scenario

You are maintaining a notification service. Alerts and notices go out through
pluggable channels (email, webhook). Channels are created from configuration by
a registry. Shapes and events flow through visitors. Background work is queued
as heterogeneous tasks, and per-request metadata lives in a property bag.
Each part below isolates one C++ technique you would use in that service.

## Part 1 - Interfaces and the Non-Virtual Interface idiom

`Channel` exposes a non-virtual `send()` that counts messages and delegates to a
private pure virtual `do_send()`. Subclasses `EmailChannel` and `WebhookChannel`
implement `do_send()`. `Notice` and `Alert` are a small value-like hierarchy with
`clone()` for deep copies.

Concepts: NVI, pure virtual interfaces, protected copy/move in polymorphic bases,
virtual `clone()`, why the base constructor cannot dispatch to a derived override.

## Part 2 - Ownership and the registry

- `Owned<T>`: a value-semantic owner of a polymorphic `T` through `unique_ptr`.
  Copying deep-copies via `clone()`. Copy assignment clones first, so a throwing
  clone leaves the target untouched.
- `ChannelRegistry`: maps names to abstract `ChannelFactory` objects. `add()`
  rejects null and duplicate factories. `create()` reports unknown names.
  `names()` is sorted.

Concepts: `unique_ptr` as the owning member of a polymorphic value, strong
exception guarantee in assignment, heterogeneous lookup with `std::less<>`,
error messages that name the offending input.

## Part 3 - CRTP mixins

- `Comparable<D>`: derives `==` and `<=>` from `D::sort_key()` through hidden
  friends, so ADL finds them and they are not members of the mixin.
- `Printable<D>`: `to_string()` and `operator<<` delegate to `D::describe()`.
- `InstanceCounter<D>`: counts live objects per `D` through a per-instantiation
  static, updated by constructors and the destructor.

Concepts: CRTP static dispatch, hidden friends, why per-`D` state needs a
separate instantiation, copy and move counting.

## Part 4 - Typed fields and model validation

- `Field<T>`: a named value with an ordered list of rules
  (`std::function<std::expected<void, std::string>(const T&)>`). `set()` is
  transactional. `validate()` reports the first failing rule.
- `Model<D>`: a CRTP base that walks `D::schema`, a `std::tuple` of pointers to
  `Field<T>` members, with `std::apply` and a short-circuit fold. The first
  failing field is reported as `"<field>: <reason>"`.

Concepts: `std::expected` for recoverable errors, `std::function` for rule
objects, pointer-to-member tuples, `std::apply`, short-circuit folds.

## Part 5 - Type erasure: Task and PropertyBag

- `Task`: a move-only, type-erased unit of work. It accepts move-only callables
  and objects with `run()` (`run()` wins when both exist). A moved-from `Task`
  throws `std::logic_error` on `run()`. Exceptions from the callable propagate,
  and the task stays usable.
- `PropertyBag`: string-keyed `std::any` storage. Typed `get<T>()` returns
  `const T*` and yields `nullptr` on a missing key or a type mismatch.

Concepts: small-buffer-free type erasure with `unique_ptr<Concept>`, `Model<U>`
adapters, `requires` expressions, `if constexpr` dispatch, `std::any_cast` with
pointers, transparent map lookup.

## Part 6 - Visitors and variants

Two dispatch styles for the same shapes:

- `Shape` hierarchy with `accept()` and a `ShapeVisitor` (classic double
  dispatch). `AreaVisitor` accumulates the total.
- `ShapeValue = std::variant<Circle, Rect>` with `std::visit` and `Overloaded`
  lambdas. `describe()` uses `std::format`.
- `circle_radius()` uses `dynamic_cast` on a base reference and returns
  `std::optional<double>`.

Concepts: double dispatch, `std::variant` and `std::visit`, the `Overloaded`
idiom with a deduction guide, `dynamic_cast` on references versus pointers, the
trade-off between open and closed hierarchies.

## Part 7 - Lazy proxy and the virtual-base diamond

- `Lazy<T>`: builds its target on first access through `operator->` or
  `operator*`, which are `const` and fill a `mutable` cache. A throwing factory
  leaves the proxy unloaded and uncounted. A factory returning `nullptr` throws
  `std::runtime_error`. `reset()` discards the target.
- `Device` / `Reader` / `Writer` / `ReadWriter`: `Reader` and `Writer` inherit
  `Device` virtually, so `ReadWriter` has one shared `Device` subobject. The most
  derived class initialises that virtual base.

Concepts: `mutable` members for logical constness, `unique_ptr` conversions to
base types, virtual inheritance and its construction order, the diamond problem.

## Concepts exercised

NVI, pure virtual interfaces, `clone()`, `unique_ptr` ownership, strong exception
guarantees, `std::less<>` heterogeneous lookup, CRTP, hidden friends, `std::expected`,
`std::function`, `std::tuple` of member pointers, `std::apply`, move-only type
erasure, `requires` expressions, `std::any`, `std::variant`, `std::visit`,
`Overloaded`, `dynamic_cast`, `mutable` caches, virtual inheritance.

## Run

Exercises (every test is expected to fail until the stubs are implemented):

```bat
tools\vsenv.cmd cmake -S . -B build/08 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCPPREF_ONLY=08_polymorphism_and_type_erasure -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=./cppref/build/_gtest-src
tools\vsenv.cmd cmake --build build/08
tools\vsenv.cmd ctest --test-dir build/08 -j8 --output-on-failure
```

Single part:

```bat
tools\vsenv.cmd ctest --test-dir build/08 -R "08_polymorphism_and_type_erasure\.Part5" --output-on-failure
```

Reference solutions (fresh build directory, with `-DCPPREF_SOLUTIONS=ON`):

```bat
tools\vsenv.cmd cmake -S . -B build/08-sol -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCPPREF_ONLY=08_polymorphism_and_type_erasure -DCPPREF_SOLUTIONS=ON -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST=./cppref/build/_gtest-src
tools\vsenv.cmd cmake --build build/08-sol
tools\vsenv.cmd ctest --test-dir build/08-sol -j8 --output-on-failure
```

## Stretch ideas (untested)

- Give `Owned<T>` a small-buffer optimisation and measure allocations.
- Make `Task` non-allocating for callables up to a fixed size.
- Replace `ChannelRegistry` factories with a `std::function` table and compare the API.
- Add a `std::generator`-free iteration over `PropertyBag` keys with `std::ranges`.
- Benchmark `dynamic_cast` against `std::variant` dispatch for `circle_radius()`.
