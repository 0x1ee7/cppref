# 03 — Namespaces and linkage

## Scenario

You own a small order-processing library shared by three services. Its public surface must
evolve without breaking callers, its diagnostics must be cheap and consistent across
translation units, its formatting helpers must resolve correctly through ADL, its
callbacks must not dangle, and its spreadsheet-style pricing formulas must evaluate safely.
Each Part is one slice of that library. The `*.hpp` files are the public surface; the `*.cpp`
files are yours to implement.

## How to work

Every engineer-owned file (`*.hpp` and `*.cpp` that are not `*_test.cpp`) currently throws
`std::logic_error("not implemented: …")`. Replace the bodies and add private members as
noted in the headers. Read the tests as the spec: they call the public API only.

Several Parts require a choice that the tests cannot see. Make the choice deliberately and
document it in a comment.

## Part 1 — Inline namespaces, ADL and a JSON encoder (`versioned_api.*`)

- `orders::v2` is an **inline namespace**, so `orders::Order` and unqualified `describe(o)` /
  `to_json(o)` resolve to v2. `orders::v1` is not inline and remains reachable only as
  `orders::v1::…`.
- `to_json` and `describe` are free functions in each version's namespace, found by **ADL**.
- `json::quote(s)` returns a JSON string literal: escape `"` and `\`; short escapes for
  `\n \t \r \b \f`; `\u00XX` (lower-case hex) for other control characters below 0x20.
- `json::dump(span)` joins already-encoded values into `[a,b,c]`. An empty span gives `[]`.
- `to_json` must use `json::quote` for every string field.

Edge cases: a quote inside an id must be escaped in the JSON output.

## Part 2 — Internal and external linkage, inline definitions (`linkage.*`)

- `kMaxAuditLine` is an **inline constexpr** variable: one object across all TUs.
- `hit_counter()` is an **inline function** defined in the header with a function-local
  `static`. It returns the post-increment value.
- `audit_line(component, message)` returns `"[component] message"`. If the result is longer
  than `kMaxAuditLine`, it is cut to `kMaxAuditLine` characters and its last three
  characters become `"..."`. A line of exactly `kMaxAuditLine` characters is not truncated.
  Put the formatting helper in an **anonymous namespace** in `linkage.cpp`, so it has
  internal linkage and is not part of the public surface.
- `error_catalog()` is **construct-on-first-use**: a function-local static `ErrorCatalog`.
  It must contain exactly 400 "Bad Request", 404 "Not Found" and 500 "Internal Server Error".

Edge cases: `find` for an unknown code returns `std::nullopt`; `error_catalog()` returns the
same object on every call.

## Part 3 — Function-local statics (`static_locals.*`)

- `crc32` implements CRC-32 (IEEE, reflected, poly `0xEDB88320`, init and final XOR
  `0xFFFFFFFF`). The 256-entry table lives in a **function-local static**, which C++11 makes
  thread-safe. `crc_table_build_count()` must report exactly one build after first use, even
  when threads race on that first use. Count the build from inside the initialiser.
- `AuditLog::instance()` is a **Meyers singleton** with a private constructor. `append`,
  `lines` and `clear` are thread-safe. `lines` returns a snapshot copy. Copy construction and
  assignment are deleted.

Edge cases: `crc32("")` is `0`; `crc32("123456789")` is `0xCBF43926`.

## Part 4 — ADL, niebloids and name hiding (`adl.*`, `hiding.*`)

**4a — ADL and customisation points (`adl.*`).**

- `shapes::Point` has `shapes::swap(Point&, Point&)` and `shapes::to_string(const Point&)`.
  Both are found by ADL, not by qualified call. `to_string` returns `"(x, y)"` using
  `std::format`'s default double formatting, so `Point{1, 2}` gives `"(1, 2)"`.
- `adl::swap_values` is a **niebloid** (a function object). It calls `swap` unqualified after
  `using std::swap;`, so a type's own swap wins and `std::swap` is the fallback.
- `adl::Textual<T>` is a concept: `to_string(t)` (unqualified, found by ADL) is convertible to
  `std::string`. `adl::text_of` is a niebloid constrained by `Textual`.

**4b — Name hiding (`hiding.*`).**

- `Formatter::format(int)` returns `"#<value>"`.
- `QuotedFormatter::format(std::string_view)` returns `"\"<text>\""`.
- Declaring `format(std::string_view)` in the derived class **hides** `Formatter::format(int)`.
  Restore it with a `using Formatter::format;` declaration in `QuotedFormatter`, so that
  `q.format(42)` compiles. The test `UsingDeclarationRestoresIntOverloadOnDerived` checks this
  through `CanFormatInt`.

Edge cases: `Formatter` must have a virtual destructor, so deleting a `QuotedFormatter`
through a `Formatter` pointer is well defined.

## Part 5 — Lambda captures and lifetimes (`captures.*`)

- `make_id_factory(start)` returns a closure that owns its counter by value.
- `make_incrementer(target)` captures `target` **by reference**. The caller must keep
  `target` alive; the header says so.
- `make_step_counter(start, step)` keeps mutable state in the closure, so copies of the
  returned `std::function` advance independently.
- `make_allow_list(vector)` takes the vector by value and captures it by value. The caller
  may mutate or destroy its own copy.
- `Scheduler::make_adder()` must **not** capture `this`. Capture `base_` by value, so the
  returned function stays valid after the `Scheduler` is destroyed. Store the base in a
  private member.

## Part 6 — RAII guards and self-registration (`hooks.*`, `plugins.*`)

**6a — Scoped hooks (`hooks.*`).**

- `emit_log(msg)` calls the innermost installed sink, or does nothing if none is installed.
- `ScopedLogHook(sink)` installs the sink and remembers the previous one. Its destructor
  restores that previous sink. Guards nest like a stack. The class is neither copyable nor
  movable. Store the previous sink in a private member.

**6b — Registry and self-registration (`plugins.*`).**

- `PluginRegistry::global()` is a function-local static, so it is safe to use from static
  initialisers in any TU.
- `add(name, factory)` throws `std::invalid_argument` on a duplicate name.
  `create(name)` returns `nullptr` for an unknown name. `names()` returns the names sorted.
- `Registrar` is constructed at namespace scope in `plugins_registration_test.cpp`, a
  separate TU. Its constructor must call `PluginRegistry::global().add(...)`. The
  registration test checks that `static_echo` is available before `main`.

Edge cases: the stub `Registrar` constructor is empty on purpose. A throw from a
namespace-scope constructor would terminate the program before `main`.

## Part 7 — A formula evaluator (`formula.*`)

- `Environment` keeps **two separate tables**, one for cells and one for functions. A cell
  named `SUM` and a function named `SUM` do not conflict: `SUM` alone is the cell, `SUM(…)`
  is the function. Store them in `std::map<std::string, …, std::less<>>` so lookups accept
  `std::string_view`.
- `set_cell` overwrites an existing value. `define_function` takes an optional arity;
  `std::nullopt` means variadic, including zero arguments. `find_function` returns the
  arity and the implementation (`FunctionRef`).
- `standard_environment()` defines `SUM` (variadic), `ABS` (1 argument), `MIN` (2) and `MAX`
  (2).
- `evaluate(expr, env)` parses with a **recursive-descent** parser into an AST, then
  evaluates. Grammar (also in the header):
  `expr := term (('+'|'-') term)*`, `term := unary (('*'|'/') unary)*`,
  `unary := '-' unary | primary`,
  `primary := NUMBER | IDENT '(' [expr (',' expr)*] ')' | IDENT | '(' expr ')'`.
- Numbers are parsed with `std::from_chars`, so they are locale-independent.
- Errors come back as `std::expected<double, Error>`. The kinds are `syntax`,
  `unknown_cell`, `unknown_function`, `arity` and `division_by_zero`. Division by a value
  that is exactly zero is an error. Do not use exceptions for these.
- `referenced_cells(expr)` returns the distinct cell names, sorted. Function names are not
  cells.

Edge cases: `""`, `"1+"`, `"(1"`, `"1 2"`, `"SUM(1,"`, and `"1..2"` are syntax errors.
`"A1(1)"` is `unknown_function` because `A1` is a cell, not a function.

## Concepts exercised

- Inline namespaces, ADL, and namespace-scope overload sets (Part 1)
- Inline variables and functions, internal linkage via anonymous namespaces,
  construct-on-first-use globals (Part 2)
- Function-local statics: thread-safe initialisation, Meyers singletons (Part 3)
- ADL customisation points, niebloids, constrained templates and concepts (Part 4)
- Name hiding and `using` declarations; virtual destructors (Part 4)
- Lambda capture modes, closure lifetimes, mutable closure state (Part 5)
- RAII guards with stack discipline, static-initialisation order, self-registration (Part 6)
- `std::expected` error values, recursive-descent parsing, explicit environments (Part 7)

## Run

From the repository root, on Windows prefix each command with `tools\vsenv.cmd`:

```
tools\vsenv.cmd cmake --workflow --preset dev
tools\vsenv.cmd ctest --preset dev -R "03_namespaces_and_linkage\.Part2"
tools\vsenv.cmd cmake --build --preset dev --target ex03_namespaces_and_linkage_tests
tools\vsenv.cmd cmake --workflow --preset solutions
```

Each Part is a set of suites named `Part1…Part7`. Each test is a separate ctest, named
`03_namespaces_and_linkage.<Suite>.<Test>`.

## Stretch (untested)

- Make `formula` evaluate `A1:B2` range references and check for reference cycles
  (`cycle` error kind) with a depth-first search.
- Replace `ScopedLogHook`'s global sink with a thread-local chain and document the
  difference.
- Give `PluginRegistry` a `unregister(name)` and reason about static destruction order when
  a plugin outlives the registry.
