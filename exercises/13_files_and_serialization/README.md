# 13 — Files & Serialization

## Scenario

You are building the core of a small backup/sync tool. It scans a source tree
honouring ignore globs, records a manifest of `{relative posix path: size, mtime,
sha256}`, diffs two manifests, applies the diff to a mirror without ever escaping
the mirror root, persists manifests atomically as JSON, exports a CSV report full
of awkward values, and writes a versioned big-endian binary snapshot header with a
CRC-32 over its payload.

Everything lives in `cppref::files`. Tests use a per-test temporary directory
under `std::filesystem::temp_directory_path()` and remove it afterwards.

## Conventions

- Paths are `std::filesystem::path`. Manifest keys are **relative POSIX strings**
  (`"pkg/mod.cpp"`), even on Windows; `\` is rejected as a separator in keys.
- Text files are UTF-8. CSV rows end with CRLF (RFC 4180).
- Timestamps are `cppref::files::Timestamp` (`sys_time<microseconds>`, UTC).
- Errors use the types named below. Filesystem problems surface as
  `std::filesystem::filesystem_error`; bad arguments as `std::invalid_argument`.

## Part 1 — Hashing and checksums

`Sha256` is a streaming SHA-256: call `update()` any number of times, then
`finish()`. Free helpers:

- `to_hex(std::span<const std::uint8_t>) -> std::string`: lowercase, two digits per byte.
- `sha256_hex(std::span<const std::byte>)` and `sha256_hex(std::string_view)`.
- `sha256_file(path, chunk_size = 65'536) -> std::string`: streams the file in
  chunks. `chunk_size == 0` → `std::invalid_argument`. Missing file →
  `filesystem_error`.
- `crc32(std::span<const std::byte>, std::uint32_t seed = 0) -> std::uint32_t`:
  reflected CRC-32 (poly `0xEDB88320`). `crc32(b, crc32(a)) == crc32(a + b)`.
  `crc32("123456789") == 0xCBF43926`.

Edge cases: the empty message, the two-block message, one million `a`
characters, streaming splits (including one byte at a time), and file digests
that match the in-memory digest for any chunk size.

## Part 2 — Tree scan and ignore globs

```cpp
struct FileEntry { std::uint64_t size; Timestamp mtime; std::string sha256; };
using Manifest = std::map<std::string, FileEntry, std::less<>>;
```

- `glob_match(pattern, path) -> bool`: split both on `/`. `*` matches within one
  segment, `?` one character, and a whole-segment `**` matches zero or more
  segments. No brackets. `*.tmp` does not match `a/b.tmp`; `**/*.tmp` does.
- `is_ignored(relative_posix, ignore) -> bool`: a pattern **without** `/` matches
  if **any path component** matches (`"*.tmp"` drops `x/y/b.tmp`;
  `"__pycache__"` drops everything under any such directory). A pattern **with**
  `/` must match the whole path (`"logs/**"` drops `logs/deep/b.log`, keeps
  `other/logs/c.log`).
- `scan_tree(root, ignore = {}) -> Manifest`: recursive walk. Only regular files
  are included. Symlinks (to files or directories) are skipped and never
  followed. Missing root → `filesystem_error(no_such_file_or_directory)`; a file
  as root → `filesystem_error(not_a_directory)`. Mtime is floored to
  microseconds. Keys are sorted bytewise.

## Part 3 — Diff, key validation, and apply

- `diff_manifests(old, new) -> ManifestDiff { added, removed, modified }`: all
  three are sorted. `modified` means the size or digest differs. An mtime-only
  change is not a modification.
- `validate_relative_key(key)`: rejects keys that are empty, absolute, or contain
  an empty, `.`, or `..` component. A drive prefix and a leading `\` are rejected
  on every platform. Failure → `std::invalid_argument` naming the key.
- `apply_diff(diff, source, dest) -> ApplyResult { copied, deleted }`:
  - Validates every key in `added`, `modified`, and `removed` **before any disk
    change**.
  - Copies `added` and `modified` files from `source/<key>` to `dest/<key>`,
    creating parent directories. `copied` lists the added keys followed by the
    modified keys.
  - Deletes `removed` keys. A key already absent is not an error and is not
    reported in `deleted`.
  - After a deletion, removes parent directories that became empty, up to but
    **not including** `dest`.
  - No rollback: a failure midway leaves earlier changes in place. Document this.

## Part 4 — Minimal JSON and manifest documents

`Json` is a value type with `Kind { null, boolean, integer, string, array, object }`.
It has no floating-point numbers.

- Factories: `Json::null()`, `Json::boolean(bool)`, `Json::integer(std::int64_t)`,
  `Json::string(std::string)`, `Json::array(Json::Array)`, and
  `Json::object(Json::Object)`, where `Object` is a vector of `(key, value)` pairs.
  A duplicate key in `object(...)` → `JsonError`.
- Accessors: `kind()`, `as_bool()`, `as_integer()`, `as_string()`, `as_array()`,
  `as_object()`. A kind mismatch → `JsonError` (offset `npos`).
  `find(key)` returns `const Json*` (or `nullptr` when the key is absent) and
  throws on a non-object.
- `dump(indent = -1) -> std::string`: a negative indent gives compact output.
  Otherwise each member/element goes on its own line, `indent` spaces per level,
  and empty containers print as `[]` and `{}`. Object keys are sorted bytewise.
  Control characters become `\u00xx` (short forms for `\b \f \n \r \t`). Invalid
  UTF-8 in a string → `JsonError`.
- `Json::parse(text) -> Json`: strict RFC 8259 subset. Rejects empty input,
  trailing content, fractions, exponents, leading zeros, integers outside
  `int64`, raw control characters in strings, invalid escapes, lone surrogates,
  duplicate object keys, invalid UTF-8, and nesting deeper than 128 (128 is
  allowed). `\uXXXX` escapes, including surrogate pairs, decode to UTF-8.
  `JsonError::offset()` is the byte index of the offending character, or the
  input size at unexpected end of input.

Manifest documents (`manifest_store.hpp`):

- `manifest_to_json(manifest) -> std::string`:
  `{"format":1,"entries":{"<key>":{"size":N,"mtime_us":N,"sha256":"<64 lowercase hex>"}}}`.
  A malformed digest on save → `std::invalid_argument`.
- `manifest_from_json(text) -> Manifest`: validates shape, format version,
  digest shape (lowercase only), non-negative size, and each key via
  `validate_relative_key`. Malformed JSON, a wrong format version, or a missing
  field → `JsonError`.

## Part 5 — Atomic writes and manifest persistence

- `write_file_atomic(path, data)`: stage into a uniquely named temporary file in
  the **same directory** as `path`, flush and close it, then rename it onto
  `path`. On any failure the temporary file is removed and the exception
  propagates; an existing `path` is untouched. The function flushes but does not
  fsync; `std` has no portable fsync. Document this.
- `read_file_binary(path) -> std::string`: missing or unreadable →
  `filesystem_error`.
- `save_manifest_atomic(manifest, path)`: `manifest_to_json` plus a trailing
  `'\n'`, written atomically.
- `load_manifest(path) -> Manifest`: missing file → `filesystem_error`.

## Part 6 — RFC 4180 CSV report

- `csv_escape(field) -> std::string`: wraps the field in quotes only when it
  contains `,`, `"`, CR, or LF, and doubles inner quotes.
- `csv_parse(text) -> std::vector<std::vector<std::string>>`: CRLF or LF record
  ends. A final terminator does not create an extra record. Empty input yields no
  records. Quoted fields may contain commas, `""`, and embedded CRLF. Malformed
  quoting (an unterminated quoted field, or text after a closing quote) →
  `CsvError`.
- `CsvWriter(std::ostream&)` with `write_row(fields)`: escapes each field and ends
  the row with CRLF.
- `export_report(manifest, path, notes = {})`: header
  `path,size,mtime,sha256,note`, then one row per entry in path order. `mtime` is
  `YYYY-MM-DDTHH:MM:SS.ffffffZ`. A note key not in the manifest →
  `std::invalid_argument`, checked **before** the file is created.
- `import_report(path) -> std::vector<ReportRow>`: the header must match exactly.
  Malformed content → `CsvError` naming the line.

## Part 7 — Versioned big-endian snapshot header

Header layout (all integers big-endian):

| Version | Size | Fields |
|---|---|---|
| 1 | 12 bytes | magic `"CPSN"` @0, version u16 @4, flags u16 @6, entry_count u32 @8 |
| 2 | 24 bytes | the v1 fields, then created_us i64 @12, crc32(payload) u32 @20 |

The payload follows the header. `snapshot_version` is 2, which is what gets written.

- `to_big_endian<T>(value) -> std::array<std::byte, sizeof(T)>` and
  `from_big_endian<T>(bytes) -> T`: `bytes.size() != sizeof(T)` →
  `std::invalid_argument`. Signed values use their two's-complement bit pattern.
- `encode_snapshot(entry_count, created_at, payload, flags = 0)`: writes the v2
  header. `entry_count` and `flags` must fit their widths, or
  `std::invalid_argument`.
- `decode_snapshot(bytes) -> Snapshot`. Fewer bytes than the header requires →
  `SnapshotError` (message contains "truncated"); wrong magic → `SnapshotError`
  (message contains "magic"); version 1 or 2 is parsed, any other version →
  `UnsupportedVersionError` (carries the version number); a CRC mismatch on a v2
  payload → `SnapshotError` (message contains "checksum mismatch"). A v1 snapshot
  has no `created_at` and no CRC.
- `write_snapshot(path, ...)` and `read_snapshot(path)`: file wrappers over
  `write_file_atomic`, `read_file_binary`, and `decode_snapshot`.

## Concepts exercised

- `std::filesystem`: recursive iteration, symlink handling, relative paths,
  `filesystem_error` and its error codes
- Streaming hashing: a class with a `finish()` state transition, `std::span`
  input, `std::byte` vs `std::uint8_t`
- Table-driven CRC-32 and seed chaining
- Ignore rules as small, testable pure functions; `std::less<>` heterogeneous
  lookup in `std::map`
- Path-traversal defence: validate everything before touching disk
- Value-type JSON with strict parsing, recursion-depth limits, and error offsets
- Atomic write: same-directory staging, RAII cleanup guard, commit by rename
- RFC 4180 parsing as a state machine; `std::ostream` writer
- Big-endian encoding, versioned headers, CRC guard
- Exception type hierarchy (`JsonError`, `CsvError`, `SnapshotError`)

## Run

Windows (prefix every command with `tools\vsenv.cmd`, from the repository root):

```
tools\vsenv.cmd cmake --workflow --preset dev
tools\vsenv.cmd ctest --preset dev -R "13_files_and_serialization\.Part2"
tools\vsenv.cmd cmake --build --preset dev --target ex13_files_and_serialization_tests
tools\vsenv.cmd cmake --workflow --preset solutions
```

The `Part2Scan.SkipsSymlinksToFilesAndDirectories` test skips on hosts that cannot
create symlinks without elevated rights.

## Stretch (untested)

- Prune ignored directories during the walk instead of visiting them, and measure
  the difference on a large tree.
- Replace `write_file_atomic`'s flush with a real fsync via a platform-specific
  layer, behind `#ifdef`, and document the cost.
- Add a streaming `Json` parser that reads from a `std::istream` and reports its
  position, without holding the whole document in memory.
- Sanitise CSV cells that start with `=`, `+`, `-`, or `@` to block spreadsheet
  formula injection, and decide where that belongs (writer or report layer).
