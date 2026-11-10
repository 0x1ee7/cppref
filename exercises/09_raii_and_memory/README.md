# 09 - RAII and memory management

## Scenario

You are building the storage layer of a small embedded document service. It needs file
handles that never leak, a byte buffer that other code can copy and move safely, scratch
allocations with custom cleanup, a shared parse cache, a transactional key-value write
path, and a bounded connection pool whose leases are used from request handlers. Every
resource in this library is owned by exactly one object, and cleanup happens on every exit
path, including exceptions.

Work in `exercises/09_raii_and_memory/`. The headers and `.cpp` files declare the full
public API. Each definition currently throws `std::logic_error("not implemented: ...")`.
The tests in `*_test.cpp` are the specification.

## Parts

### Part 1 - Handles (`unique_handle.hpp`, `file_handle.cpp`)
Implement `unique_handle<Traits>`, a move-only RAII wrapper parameterised by a traits
policy (`handle_type`, `invalid()`, `close()`). Then implement `open_file`, which returns
`std::expected<FileHandle, std::error_code>` and never leaks on failure.
Concepts: sentinel-based ownership, `reset`/`release` semantics, self-move, `noexcept` moves.

### Part 2 - Rule of five (`buffer.hpp`, `buffer.cpp`)
Implement `ByteBuffer`: deep-copy constructor and assignment, `noexcept` moves that leave
the source empty, and `append` with the strong exception guarantee.
Concepts: rule of five, copy-and-swap versus direct copy, self-assignment, growth policy.

### Part 3 - Unique ownership with a stateful deleter (`buffer.hpp`)
Implement `ScratchPtr` (`std::unique_ptr<int[], ScratchDeleter>`) and `make_scratch`.
`ScratchDeleter` counts frees through a pointer. The deleter must survive moves.
Concepts: `unique_ptr` deleter as part of the type, empty-base optimisation, `release` and
`reset` contracts.

### Part 4 - Shared ownership and weak caching (`document.hpp`, `document.cpp`)
Implement `Document::parse` (returns `shared_ptr<const Document>`), `Section` access, the
aliasing constructor in `body_of`, and `DocumentCache`. The cache holds only weak references,
so it never keeps a document alive. `get_or_parse` reparses an expired entry.
Concepts: `shared_ptr` versus `weak_ptr`, aliasing constructor, cycle avoidance, expiry checks.

### Part 5 - Scope guards and temporary directories (`guards.hpp`, `temp_dir.hpp`, `temp_dir.cpp`)
Implement `scope_exit`, `scope_fail`, and `scope_success` as RAII guards that run a callable
on destruction. The `fail` and `success` variants compare `std::uncaught_exceptions()`.
Implement `TempDir`, which creates a unique directory and removes it recursively on destruction.
Concepts: scope guards, `uncaught_exceptions`, `noexcept` destructors, filesystem cleanup.

### Part 6 - Transactions (`transaction.hpp`, `transaction.cpp`)
Implement `Transaction`, an undo-log transaction over a `KvMap`, and `Savepoint`, a move-only
partial-rollback marker. Destroying an uncommitted transaction rolls it back.
Concepts: undo journals, strong guarantees, non-movable RAII owners, nested rollback.

### Part 7 - Pool leases and arena allocation (`connection_pool.*`, `arena_parser.*`)
Implement `ConnectionPool`. A `Lease` is a move-only handle that returns its connection on
destruction, discards broken connections, and may outlive the pool through shared state.
Implement `tokenize`, which returns `std::expected<std::pmr::vector<Token>, ParseError>`. Every
token string and the vector storage come from the caller's `memory_resource`.
Concepts: move-only leases, `shared_ptr` for shared state, `polymorphic_allocator`,
uses-allocator construction (including allocator-extended copy and move constructors).

## Concepts exercised

- RAII and exception-safe cleanup on every exit path
- Rule of zero, rule of five, and `noexcept` moves
- Sentinel-based resource traits and move-only handles
- `unique_ptr` with stateful deleters
- `shared_ptr`, `weak_ptr`, and the aliasing constructor
- Scope guards with `std::uncaught_exceptions`
- Undo-log transactions and strong exception guarantees
- `std::pmr` allocators and uses-allocator construction

## Run

On Windows, prefix each command with `tools\vsenv.cmd`.

```
cmake --workflow --preset dev
ctest --preset dev -R "09_raii_and_memory\.Part2"
cmake --build --preset dev --target ex09_raii_and_memory_tests
cmake --workflow --preset solutions
```

Every test fails against the stubs. Use `ctest --preset dev` to see the failures in your
own implementation as you go.

## Stretch ideas (untested)

- Make `unique_handle` work with a non-trivially-copyable handle type (for example a `std::string` name).
- Add `std::generator`-style iteration over `Document` sections once the toolchain supports it.
- Replace `DocumentCache` with a thread-safe variant using `std::shared_mutex`, and test it under `--repeat until-fail`.
- Add a `Transaction` nested-savepoint stress test that checks the journal length after random operations.
