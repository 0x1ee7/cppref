# 12 · Logging library

## Scenario

You own the logging layer of a C++ service that ships as a library and runs as
an application. Library code must log without configuring anything. The
application decides where records go, at what level, in what format, and how
secrets are kept out of files. Records must never stall request threads: a slow
disk or a full queue has to degrade by dropping records, not by blocking.

This is the C++ sibling of pyref's `10_logging`. The philosophy carries over:
libraries log through named loggers and never install handlers, applications
own the configuration, and secrets are redacted before they reach disk. The API
is C++ throughout: value-semantic records, RAII scopes, `std::format`-checked
call sites, `std::expected` for parsing, and explicit ownership of sinks.

The library lives in `cppref::logging`. It has no third-party dependencies.

## Files

| File | Role |
|------|------|
| `logging.hpp` / `logging.cpp` | Levels, `Record`, `Sink`, MDC, `Logger`, `Registry`, call-site macros |
| `sinks.hpp` / `sinks.cpp` | Formatters, `OstreamSink`, `RotatingFileSink` |
| `async.hpp` / `async.cpp` | `AsyncSink`: bounded queue and a worker thread |
| `redact.hpp` / `redact.cpp` | `redact()`, `is_secret_key()`, `RedactingSink` |
| `test_support.inc` | Test helpers (not compiled on its own) |
| `*_test.cpp` | Tests; these are the spec |

## Part 1 · Levels and hierarchy

Implement `parse_level`, `to_string`, and the `Registry`/`Logger` hierarchy.

- `Level` is `trace < debug < info < warn < error < off`.
- `parse_level` is case-insensitive, accepts `warning`, and returns
  `std::expected<Level, ParseLevelError>` where the error carries the input.
- `Registry::root()` is the unnamed root logger, explicitly at `info`.
- `Registry::get("app.db")` returns a stable `Logger&` for a dotted name and
  creates missing ancestors (`app`) on demand. Empty names and empty segments
  (`""`, `"a..b"`, `".a"`, `"a."`) throw `std::invalid_argument`.
- A logger without an explicit level inherits the nearest ancestor's level, and
  it sees later changes made to that ancestor. Prefixes are matched by dotted
  segment only: `application` is not under `app`.
- `enabled(level)` compares against the effective level. `propagate()` defaults
  to `true`.
- Registries are independent of each other.

Edge cases: `clear_level()` restores inheritance; `Logger` references stay valid
for the life of the `Registry`.

## Part 2 · Lazy formatting, call sites, dispatch

Implement `Logger::log`'s effect through `emit`, the `Record` fields, and
dispatch up the hierarchy.

- `CPPREF_INFO(logger, "fmt", args...)` and friends check `enabled()` before
  calling `std::format`. A disabled call must not format arguments. Format
  strings are checked at compile time through `std::format_string`.
- Each record carries the call site (`std::source_location`), the logger name,
  the time, and the thread id.
- Records go to the sinks of the logger and then up the chain while
  `propagate()` holds. Each sink receives its own copy.
- Filtering by level happens before any sink sees a record.

## Part 3 · Formatters and sinks

Implement `text_format`, `json_format`, `json_escape`, `format_utc_iso8601`,
`OstreamSink`, and `RotatingFileSink`.

Text: `2023-11-14T22:13:20.123Z INFO  app.db: message k=v`. The level is padded
to five columns. Fields are appended as ` key=value`.

JSON: one object per line with `ts`, `level` (lowercase), `logger`, `msg`,
`file`, `line`, and `fields`. `json_escape` escapes quotes, backslashes, the
short escapes `\b \f \n \r \t`, other control characters as `\u00XX`. Bytes
≥ 0x80 pass through unchanged.

`OstreamSink` borrows the stream, writes one newline-terminated line per record,
and serializes writers with a mutex. `flush()` flushes the stream.

`RotatingFileSink(path, max_bytes, max_files, formatter)`:

- Appends to an existing file, and counts its current size toward the limit.
- Before a write, if the file is non-empty and `size + line > max_bytes`, it
  rotates: `.N-1` → `.N` (dropping the old `.max_files`), then `path` → `.1`.
- An oversized single line is still written (to a fresh file), never dropped.
- `max_bytes == 0` or `max_files == 0` throws `std::invalid_argument`. A failed
  open throws `std::system_error`.

## Part 4 · Asynchronous sink and concurrency

Implement `AsyncSink`.

- The constructor takes `shared_ptr<Sink> target` and a queue `capacity`
  (`capacity >= 1`, target non-null, both checked with `std::invalid_argument`).
  It starts one worker thread.
- `write()` never blocks. A full queue drops the record and increments
  `dropped()`.
- `flush()` returns only after every accepted record has been delivered
  (including one that is in flight), and then flushes the target.
- The destructor delivers the remaining records, flushes, and joins the worker.
- A throwing target `write()` is counted in `failed()` and does not kill the
  worker. Exceptions never escape the worker thread.
- The queue is owned by the sink. Concurrent producers must not lose records
  while the queue has room.

Concurrency edge cases: no interleaved lines under `OstreamSink` with many
writers; `Registry::get` must create each logger exactly once under contention.

## Part 5 · Mapped diagnostic context (MDC)

Implement `MdcScope` and `mdc_snapshot()`.

- `MdcScope(key, value)` pushes a key/value pair onto a per-thread stack and
  pops it on destruction. It is neither copyable nor movable.
- Scopes nest LIFO. An inner key shadows an outer key with the same name, and
  the outer value comes back when the inner scope ends.
- `mdc_snapshot()` returns one entry per key, the innermost value wins, and the
  order is outermost-first by first insertion.
- Other threads do not see the scope.
- The snapshot is taken on the logging thread at log time. `AsyncSink` delivers
  later, but the record already carries the fields.
- Scopes unwind correctly when an exception passes through.

## Part 6 · Redaction

Implement `redact()`, `is_secret_key()`, and `RedactingSink`.

- Secret keys (case-insensitive): `password`, `passwd`, `secret`, `token`,
  `api_key`, `api-key`, `access_token`, `client_secret`, `authorization`.
- In a message, a key token (a maximal `[A-Za-z0-9_-]` run) followed by `=` or
  `:` (quotes and spaces allowed around the separator) has its value replaced by
  `***`. Quotes around the value are preserved.
- Unquoted values end at whitespace, `,`, `&`, `}`, or `;`. An unquoted
  `authorization` value runs to end of line.
- Keys that merely contain a secret word (`passwordless`, `tokenizer`) are left
  alone.
- `RedactingSink` rewrites the message and masks the value of any secret-keyed
  field. It never drops records and forwards `flush()`.

## Concepts exercised

- RAII and scope-bound state: `MdcScope`, `Registry` ownership, sinks owned by
  `shared_ptr`.
- Lazy evaluation at the call site: `enabled()` before `std::format`, with
  compile-time format strings and `std::source_location`.
- Value semantics for records; `std::expected` for parse errors.
- Thread-safety design: mutexes, condition variables, bounded queue,
  back-pressure by drop, drain-on-destruction, exceptions contained at the
  thread boundary.
- Atomics for hierarchical level lookup seen by existing children.
- Stable references from a map (`std::map` with heterogeneous lookup).
- Portable file I/O: append mode, sizes, rotation, `std::filesystem`.
- Decorator sinks: `RedactingSink` wraps any sink.

## Run

Windows (prefix every command with `tools\vsenv.cmd`):

```
tools\vsenv.cmd cmake --workflow --preset dev
tools\vsenv.cmd ctest --preset dev -R "12_logging\.Part2"
tools\vsenv.cmd cmake --build --preset dev --target ex12_logging_tests
tools\vsenv.cmd cmake --workflow --preset solutions
```

Each test is a separate ctest named `12_logging.<Suite>.<Test>`. Run one part
with `-R "12_logging\.Part4"`.

## Stretch (untested)

- A `SyslogSink` that writes RFC 5424 lines to a Unix datagram socket behind
  `#ifdef`, with a portable stub on Windows.
- Per-sink level filters, so a file can keep `debug` while the console keeps
  `warn`.
- A `Formatter` that writes into a caller-supplied `std::pmr::monotonic_buffer_resource`,
  and a measurement of the allocation count on the disabled path.
- `std::stop_token`-aware `AsyncSink::flush` with a deadline.
- Structured fields as `std::variant` values (`int64_t`, `double`, `bool`,
  string) instead of strings, with type-preserving JSON output.
