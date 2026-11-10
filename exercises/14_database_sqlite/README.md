# 14 · SQLite-backed order management

## Scenario

You are building the persistence layer of an order-management backend: customers, a product
catalogue, and orders with their lines. Storage is a single SQLite file. The service needs to
place orders atomically, evolve the schema safely over time, import large catalogue feeds
without exhausting memory, page through orders with stable cursors, and answer reporting
queries. This is the C++ sibling of pyref exercise 13. The same domain, but the wrapper is
yours: no ORM, no string-concatenated SQL, no hidden global connection.

The C library is the SQLite 3.47.2 amalgamation, fetched by CMake at a pinned SHA3-256 and
compiled into `sqlite3_amalgamation`. You code against `sqlite3.h` only inside your `.cpp`
files. Your headers expose the C++ API.

## Parts

Each Part is a test suite (`Part1Database`, `Part2Binding`, ...). Tests are the spec. Keep the
public API and behaviour as the tests describe.

### Part 1 · Database and statements (`sqlite_wrapper.hpp`)

- `Database(path)` opens with `SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE` and enables
  extended result codes and `PRAGMA foreign_keys = ON`. It is move-only; a moved-from
  handle is empty. `":memory:"` is valid.
- `exec(sql)` runs a script and throws `DbError` (message and extended code from
  `sqlite3_errmsg` and `sqlite3_extended_errcode`). `try_exec` / `try_prepare` report
  failures as `std::expected<..., SqlError>`.
- `prepare(sql)` returns a move-only `Statement` that finalizes its handle. `step()` returns
  `true` for a row and `false` at completion. `reset()` and `clear_bindings()` make it
  reusable.
- `changes()` and `last_insert_rowid()` track the last write.

### Part 2 · Binding and reading (`sqlite_wrapper.hpp`)

- `bind(index, value)` and `bind_all(args...)` accept integers, floating point,
  `std::string_view` and `std::string` (bound with `SQLITE_TRANSIENT`), and `std::optional<T>`
  (`nullopt` binds NULL).
- `column<T>(i)` reads one column. Reading a NULL into a non-optional `T` throws
  `DbError` with `SQLITE_MISMATCH`.
- `row<Ts...>()` returns a `std::tuple` for a whole row. `query_all<Ts...>(db, sql, args...)`
  collects every row.
- Bound text is copied: changing the caller's buffer after `bind` must not affect the query.

### Part 3 · Transactions and savepoints (`sqlite_wrapper.hpp`)

- `Transaction(db)` runs `BEGIN IMMEDIATE`. `commit()` makes it permanent. The destructor rolls
  back if the transaction is still open, including during exception unwinding.
- A second writer on the same file gets `SQLITE_BUSY` (surfaced as `DbError`).
- `Savepoint(db, name)` nests inside a transaction. Rolling back to it keeps the outer work.
  The name must be a plain identifier (`std::invalid_argument` otherwise). Release it with
  `release()`, or the destructor rolls back to it.

### Part 4 · Schema migrations (`migrations.hpp`)

- `builtin_migrations()` returns a span of versioned migrations (v1 tables, v2 index).
  `schema_version(db)` reads `PRAGMA user_version`.
- `migrate(db, migrations)` applies pending versions in order, each in its own
  transaction together with the `user_version` bump. Re-running is a no-op.
- Versions must be exactly `1..n` (`std::invalid_argument` otherwise). A failing migration
  rolls back completely and throws `MigrationError` (with `version()`), with the original
  `DbError` attached as a nested exception (`std::rethrow_if_nested`).
- A database whose `user_version` is newer than the code is rejected with `MigrationError`.
- The shipped tables are `STRICT`, use CHECK constraints, and anchor foreign keys.

### Part 5 · Repositories (`repositories.hpp`)

- `CustomerRepository`: `add`, `get`, `get_by_email` (case-insensitive via `COLLATE NOCASE`),
  and `search_by_name`, a literal substring match. `%` and `_` in the term match literally
  (`ESCAPE`). An empty term is rejected.
- `ProductRepository`: `upsert` (`ON CONFLICT DO UPDATE`, not `REPLACE`), `upsert_many` (all
  rows validated first, one transaction), `get`. Negative prices are rejected.
- `OrderRepository::place` runs in one transaction. It snapshots unit prices from the
  catalogue and numbers lines from 1. Unknown SKUs throw `UnknownProductError` listing every
  missing SKU, sorted and unique. An unknown customer fails the foreign key check and leaves
  nothing behind. `set_status` validates the status and throws `std::out_of_range` for an
  unknown order.
- Security: SQL injection attempts through every text input are inert (bound, not
  concatenated).

### Part 6 · Bulk import (`repositories.hpp`)

- `import_products(db, rows, batch_size)` is a function template over any single-pass input
  range of `Product`-convertible values. It streams in batches, one transaction per batch,
  with memory bounded by the batch size.
- Batches already committed stay committed when a later row is invalid. The batch holding the
  bad row rolls back in full. Re-importing updates prices without duplicating rows.
- `batch_size == 0` is rejected.

### Part 7 · Order queries (`order_queries.hpp`)

- `OrderCursor` encodes `(placed_at, order_id)` as URL-safe base64 without padding.
  `decode` validates everything it reads: alphabet, length, the single `:` separator, and
  full-consumption integer parsing. Malformed input throws `std::invalid_argument`.
- `page_orders_keyset(db, after, limit)` pages in `(placed_at, id)` order. Ties on the
  timestamp are broken by id. Pages do not duplicate or skip rows when older rows are
  inserted. `next_cursor` is set only when a page is full.
- `page_orders_offset(db, offset, limit)` walks the same order by position. Its validation
  rules match the keyset pager.
- `load_orders_with_items(db, customer_id)` returns each order with its lines. It issues a
  bounded number of statements (not one per order) and groups results in code.

### Part 8 · Reports (`reports.hpp`)

- Every report excludes `cancelled` orders.
- `units_sold_by_product`: units and distinct order counts per SKU, sorted by units descending,
  then SKU.
- `revenue_by_customer`: exact integer-cents totals per customer, sorted by id. Customers
  without live orders are omitted.
- `rank_customers_by_orders`: every customer, ranked by live order count with SQL `RANK()`
  (ties share a rank and the next rank is skipped). Sorted by rank, then name.
- `customer_running_units`: per-order units plus a running total, using `SUM() OVER`.

## Concepts exercised

- RAII over C handles: move-only owners, finalize and close in destructors, no leaks on the
  error path.
- `std::expected` for recoverable failures next to exceptions for programming and
  constraint errors. Exception-safety guarantees for multi-statement operations.
- Transactions and savepoints as scope guards; rollback during unwinding.
- Parameter binding as the injection defence; `SQLITE_TRANSIENT` lifetime rules.
- Concepts and variadic templates (`bind_all`, `row<Ts...>`) over a type-safe C API.
- `std::span`, `std::ranges::input_range`, batch streaming with bounded memory.
- Nested exceptions (`throw_with_nested`, `rethrow_if_nested`) to keep the root cause.
- Money as integer cents; `std::chrono::sys_seconds` at the storage boundary.
- Keyset versus offset pagination; opaque cursors and strict decoding.
- Pinned third-party dependency via `FetchContent` with `URL_HASH`.

## Run

Windows (MSVC): prefix every command with `tools\vsenv.cmd`, run from the repository root.

```
tools\vsenv.cmd cmake --workflow --preset dev
tools\vsenv.cmd ctest --preset dev -R "14_database_sqlite\.Part2"
tools\vsenv.cmd cmake --build --preset dev --target ex14_database_sqlite_tests
```

Reference solutions:

```
tools\vsenv.cmd cmake --workflow --preset solutions
```

## Stretch ideas (untested)

- Switch to WAL mode and open a read-only connection pool; measure reader/writer concurrency
  with `SQLITE_BUSY` retry policy (`sqlite3_busy_timeout`) wrapped in your API.
- Add an LRU statement cache keyed by SQL text, with `Statement` leases that cannot outlive
  the cache.
- Add a `ProductRepository::stream_all` that yields rows lazily as a `std::generator` once
  your toolchain provides it.
- Replace `PRAGMA user_version` with a `schema_migrations` table holding checksums; detect
  edited migrations.
- Add an `SQLITE_CHANGESET` based replication of order writes to a second file.
