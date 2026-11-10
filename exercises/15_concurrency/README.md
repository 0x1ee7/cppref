# 15 — Concurrency

## Scenario

You own the backend of a small internal service suite. Five components need concurrency done
properly, and each one has a classic way to go wrong:

- a **link checker** that probes thousands of URLs for a documentation site,
- a **bounded downloader** that must respect a per-host connection budget,
- a **billing ledger** with shared counters, accounts that move money, and a route table
  hot-reloaded while readers are active,
- a **cache** in front of an expensive lookup, where a thundering herd must not hit the
  backend once per request,
- a **job pipeline** (producer → transform → sink) with backpressure, plus a **thread pool**
  and an **SPSC ring** used by a logging hot path, and a **periodic scheduler** for
  maintenance tasks.

The Python reference (`pyref/14_threads_and_processes`) uses threads and processes. Here the
emphasis is C++20/23 threading: `std::jthread` and `std::stop_token`, `std::mutex` with
`scoped_lock`, `condition_variable_any` with stop-aware waits, `std::counting_semaphore`,
`std::shared_future`, `std::packaged_task`, and lock-free atomics with explicit memory orders.

Every operation in this exercise is observable through the public API. The tests are the
specification: they bound every wait, join every thread, and fail (not hang) against the stubs.

## Parts

### Part 1 — Concurrent link checker (`check_links`)

```cpp
struct LinkResult { std::string url; std::optional<int> status; std::optional<std::string> error;
                    bool ok() const; bool operator==(const LinkResult&) const = default; };
using Fetch = std::function<int(std::string_view, std::stop_token)>;
std::vector<LinkResult> check_links(std::span<const std::string>, const Fetch&,
                                    std::stop_token, std::size_t max_workers = 8);
```

- Results come back in input order, one per input URL.
- `ok()` is true for statuses in `[200, 400)`.
- A fetch exception becomes `error` (its `what()` text).
- Duplicate URLs are fetched once but each input position gets a result.
- Never more than `max_workers` fetches run at once. `max_workers == 0` is `invalid_argument`.
- Stop before start: everything is `"cancelled"`. Stop mid-run: unstarted URLs are
  `"cancelled"`; in-flight fetches are awaited before the call returns.

### Part 2 — Bounded downloader (`BoundedDownloader`)

```cpp
BoundedDownloader(Fetcher fetch, std::size_t max_concurrent);
std::string download(std::string_view url);
std::vector<std::string> download_all(std::span<const std::string> urls);
std::size_t in_flight() const;
std::size_t peak_concurrency() const;
```

- A `counting_semaphore` enforces the cap; a slot is released even when the fetch throws.
- `max_concurrent` must be in `[1, counting_semaphore::max()]` (else `invalid_argument`).
- `download_all` keeps input order. If several fetches fail, the lowest-index exception is
  rethrown after all workers have finished.
- `peak_concurrency()` never exceeds the cap.

### Part 3 — Shared state (`Counter`, `Account`/`transfer`, `RouteTable`)

- `Counter::increment(name, by)` is atomic per call; `by < 0` is `invalid_argument`.
  `snapshot()` returns a consistent copy.
- `Account` owns a mutex and is not copyable. `deposit`/`withdraw` require amount > 0;
  `withdraw` beyond the balance throws `InsufficientFunds` and changes nothing.
- `transfer(src, dst, amount)` locks both accounts together (`std::scoped_lock`), so opposing
  transfers cannot deadlock. Same account or amount ≤ 0 is `invalid_argument`.
- `RouteTable::replace` swaps a whole table atomically; readers see the old table or the new
  one, never a mix (`shared_mutex`, readers in parallel).

### Part 4 — Caches (`BoundedCache`, `SingleFlightCache`)

- `BoundedCache<K, V>` is an LRU: `get` and `put` refresh recency, `contains` does not.
  `hits()`/`misses()` count `get` calls. Capacity 0 is `invalid_argument`.
- `SingleFlightCache<K, V>::get_or_load(key, loader)`: concurrent misses on the same key share
  one loader call (single flight). The loader runs *without* the cache lock, so a slow key does
  not block other keys. A loader failure reaches every waiter and nothing is cached.
- `load_count()` counts loader invocations; it is the observable proof of single flight.

### Part 5 — Bounded queue, multi-stage pipeline

- `BoundedQueue<T>`: `push` blocks when full, `pop` blocks when empty. `close()` lets buffered
  items drain, then `pop` returns `nullopt`. A `std::stop_token` passed to `push`/`pop` wakes
  the call immediately without losing data.
- `run_pipeline(inputs, stages, workers_per_stage, queue_capacity)`: each stage runs its own
  worker threads, connected by bounded queues (backpressure). Output keeps input order even when
  items finish out of order. A throwing stage tears the pipeline down, joins every thread, and
  rethrows the first exception.

### Part 6 — Thread pool and futures

- `ThreadPool(n)`: `submit(f)` returns `std::future<R>` carrying the result or exception.
  Destruction drains the queue.
- `shutdown(ShutdownMode::drain)` runs queued work; `shutdown(ShutdownMode::cancel)` drops queued
  work (those futures throw `std::future_error` with `broken_promise`) and lets running tasks
  finish. `submit` after shutdown is `runtime_error`. `shutdown` is idempotent.
- `run_all(jobs)`: one `std::async` per job, results in order; the lowest-index failure is
  rethrown after every job has finished. Empty input returns an empty vector.

### Part 7 — Lock-free SPSC ring and periodic scheduler

- `SpscRing<T, Capacity>`: single-producer single-consumer ring with acquire/release atomics.
  `Capacity` must be a power of two. `try_push` leaves its argument untouched when full.
  The destructor destroys leftover items. Move-only payloads must work.
- Stress test: 200 000 items arrive in order across two threads.
- `PeriodicScheduler`: `add` before `start`; jobs run on their own threads at their interval.
  `stop()` interrupts sleeping jobs immediately, is idempotent, and is a no-op before `start`.
  A throwing job records a `JobError` and keeps running.

## Concepts exercised

- `std::jthread`, `std::stop_token`, `std::stop_callback`, `std::stop_source`: cooperative
  cancellation, and jthread's join-on-destroy as RAII.
- `std::mutex`, `std::lock_guard`, `std::scoped_lock` (deadlock-free multi-lock), `std::shared_mutex`.
- `std::condition_variable` vs `std::condition_variable_any` (stop-aware waits) and the
  lost-wakeup window around predicate checks.
- `std::counting_semaphore` for resource caps.
- `std::atomic` with acquire/release ordering; compare-exchange loops; cache-line alignment.
- `std::future`, `std::shared_future`, `std::promise`, `std::packaged_task`, `std::async`,
  broken promises and exception propagation through futures.
- Single-flight loading, LRU bookkeeping, queue-based backpressure, thread-pool lifetime rules.

## Run

Windows (MSVC): prefix commands with `tools\vsenv.cmd`.

Exercises (the stubs must fail — that is the starting state):

```
tools\vsenv.cmd cmake --workflow --preset dev
tools\vsenv.cmd ctest --preset dev -R "15_concurrency\.Part2"
tools\vsenv.cmd cmake --build --preset dev --target ex15_concurrency_tests
```

Reference solutions:

```
tools\vsenv.cmd cmake --workflow --preset solutions
```

## Stretch ideas (untested)

- Replace `SingleFlightCache`'s `std::shared_future` with a `std::latch`-based waiter and compare
  the wake-up behaviour under contention.
- Add a `ShutdownMode::graceful_timeout` to `ThreadPool` that waits a bounded time before cancelling.
- Implement a `WorkStealingPool` with per-thread deques and measure it against `ThreadPool`.
- Give `SpscRing` a batched `try_push_n`/`try_pop_n` and benchmark it.
- Add a `std::stop_token`-aware `PeriodicScheduler` variant driven by `std::chrono::steady_clock`
  deadlines instead of fixed intervals, to handle slow jobs without drift.
