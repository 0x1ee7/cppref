# 17 · `<chrono>`: durations, calendars, time zones

Sibling of pyref `17_dates_and_times`.

## Scenario

You are building the time layer of a scheduling and billing backend. Requests arrive
with epoch timestamps (integer seconds, milliseconds, or floats), ISO 8601 strings, and
RFC 2822 headers. Customers live in different IANA zones, meetings recur weekly across
DST transitions, invoices bill on the anchor day of the month, and a worker pool
measures elapsed time for SLA reports. Every rule here is a production rule: a
misplaced floor, a shifted wall clock, or a `system_clock` used for timing shows up as
a wrong invoice or a failed SLA.

## Files

| File | Role |
| --- | --- |
| `common.hpp` | `Instant` (`sys_time<milliseconds>`), `ZonedInstant`, `UnknownZoneError` |
| `durations.hpp/.cpp` | Part 1: floor, truncate, round, epoch conversion |
| `parse.hpp/.cpp` | Part 2: epoch, ISO 8601 and RFC 2822 parsing |
| `zones.hpp/.cpp` | Part 3: zone lookup, display, local-time resolution, DST-aware arithmetic |
| `calendar.hpp/.cpp` | Part 4: dates, month arithmetic, billing dates, business days |
| `meetings.hpp/.cpp` | Part 5: recurring meetings with explicit gap and overlap policies |
| `slots.hpp/.cpp` | Part 6: cross-zone time slots, merging, total booked time |
| `humanize.hpp/.cpp` | Part 7a: relative time strings |
| `stopwatch.hpp` | Part 7b: `Stopwatch` over an injected steady clock |
| `*_test.cpp`, `test_support_test.hpp` | Tests (spec), fixtures |

The engineer-owned files are the non-test `.hpp`/`.cpp` files. Every function body is
a stub that throws `std::logic_error("not implemented: <name>")`, so every test fails
until you implement it. Public signatures and doc comments are the contract; the tests
and their expectations are the spec. The `Stopwatch` and `BusinessCalendar` private
members are left for you.

## Part 1: durations, rounding, epoch milliseconds

API (`durations.hpp`):

- `std::int64_t to_epoch_ms(system_clock::time_point)`
- `Instant to_millisecond_instant(system_clock::time_point)`
- `seconds floor_seconds(milliseconds)`, `truncate_seconds(milliseconds)`, `round_seconds(milliseconds)`

Edge cases:

- `floor_seconds(-1500 ms) == -2 s`, while `truncate_seconds(-1500 ms) == -1 s`. Use floor for bucket boundaries.
- `round_seconds` uses ties to even: `2500 ms -> 2 s`, `3500 ms -> 4 s`.
- Sub-millisecond instants floor toward the past. `-1 us` becomes `-1 ms`, not `0`. `system_clock` has a 100 ns tick on MSVC, so the tests use microseconds to construct exact values.

Concepts: `duration_cast` vs `floor`/`round`, integer rounding direction, `time_point` construction, `std::chrono` ratio arithmetic.

## Part 2: parsing

API (`parse.hpp`):

- `Instant parse_timestamp(std::string_view)` accepts a numeric epoch or an ISO 8601 / RFC 2822 string.
- `Instant from_epoch_integer(std::int64_t)`: magnitude ≥ `100'000'000'000` is milliseconds, otherwise seconds.
- `Instant from_epoch_float(double)`: fractional values round to the nearest millisecond (ties away from zero). NaN and infinities throw `std::invalid_argument`.

Edge cases:

- ISO timestamps require an offset or `Z`. `2023-11-14 22:13:20.250Z` (space separator, fraction) is accepted.
- A calendar-impossible date such as `2026-02-30T10:00:00Z` throws `std::invalid_argument`. `sys_time` normalises it silently, so validate the fields first.
- RFC 2822 `-0000` is rejected; the offset must be known.
- Years outside 0001–9999 (UTC) throw `std::out_of_range`.
- Empty, garbage, and whitespace-only input throws `std::invalid_argument`.

Concepts: `std::chrono::from_stream` with `%FT%T%Ez` and `%a, %d %b %Y %H:%M:%S %z`, `std::from_chars`, strict whole-string validation, `year_month_day::ok()`.

## Part 3: zones and DST-aware arithmetic

API (`zones.hpp`):

- `const time_zone* find_zone(std::string_view)`: throws `UnknownZoneError`.
- `ZonedInstant in_zone(Instant, std::string_view zone)`
- `std::string format_in_zone(Instant, std::string_view zone)`: `"2026-03-29 14:00 CEST"`.
- `Instant localize(local_time<milliseconds>, std::string_view zone, choose)`
- `Instant localize_strict(local_time<milliseconds>, std::string_view zone)`: lets the standard library throw `ambiguous_local_time` / `nonexistent_local_time`.
- `ZonedInstant add_elapsed(const ZonedInstant&, milliseconds)`: absolute time; wall fields can shift across DST.
- `ZonedInstant add_calendar_days(const ZonedInstant&, days)`: keeps the wall-clock time and zone; ambiguous results take the earlier occurrence.
- `milliseconds elapsed_between(Instant start, Instant end)`: signed `end - start`.

Edge cases:

- Berlin DST began at 01:00 UTC on 2026-03-29, so 12:00 UTC shows as 14:00 CEST.
- Do not assert the `choose` behaviour on nonexistent wall times in tests; the standard only defines it for ambiguous ones.

Concepts: `time_zone`, `zoned_time`, `local_time`, `choose`, `get_info`, the tzdb, and the difference between calendar and elapsed arithmetic.

## Part 4: calendars, billing dates, business days

API (`calendar.hpp`):

- `unsigned days_in_month(year_month)`
- `year_month_day parse_date(std::string_view)`: strict `YYYY-MM-DD`.
- `year_month_day add_months(year_month_day, months)`: clamps to the end of the target month.
- `std::vector<year_month_day> billing_dates(year_month_day anchor, int count)`: computed from the anchor, not iteratively.
- `year_month_day last_weekday(year_month, weekday)`: e.g. the last Monday of May.
- `class BusinessCalendar`: constructed from holidays and a weekend set; `is_business_day`, `add_business_days`, `business_days_between`.

Edge cases:

- `2026-01-31` + 1 month = `2026-02-28`; the clamp does not stick. The billing series from `2026-01-31` is `2/28`, `3/31`, `4/30`.
- Negative `count` throws; `count == 0` returns an empty vector.
- Holidays that are impossible dates throw. A weekend set that covers every weekday throws (no business day exists).
- `business_days_between` is signed.

Concepts: `year_month_day`, `year_month_day_last`, `month_day_last`, `weekday_indexed` (`wd[last]`), `std::set` of calendar dates, value semantics for the calendar.

## Part 5: recurring meetings with DST policies

API (`meetings.hpp`):

- `enum class Gap { skip, shift_forward }`: policy for nonexistent local times.
- `enum class Overlap { earlier, later, skip }`: policy for ambiguous local times.
- `struct WeeklyRule { year_month_day start; weekday day; minutes at; std::string zone; std::size_t count; Gap gap; Overlap overlap; }`
- `std::vector<Instant> weekly_occurrences(const WeeklyRule&)`: ascending UTC instants.

Edge cases:

- A Monday 09:00 meeting in Berlin keeps 09:00 local across DST; the UTC instant moves from 08:00Z to 07:00Z.
- `shift_forward`: a 02:30 meeting on the spring-forward Sunday happens at 01:30Z (03:30 CEST).
- `skip` drops the occurrence and it does not count toward `count`.
- Overlap `earlier` picks 00:30Z on the fall-back day, `later` picks 01:30Z.
- `count == 0` gives an empty vector. Invalid `at` (outside `[0h, 24h)`) and invalid start dates throw `std::invalid_argument`; unknown zones throw `UnknownZoneError`.

Concepts: policy-as-enum design, `std::chrono::local_info`, a `switch` over the `local_info` result, weekday arithmetic, and the difference between a wall-clock rule and an instant.

## Part 6: cross-zone slots

API (`slots.hpp`):

- `TimeSlot`: a `ZonedInstant` start and end with `start()` and `end()` accessors. Inverted or empty slots are rejected at construction.
- `bool overlaps(const TimeSlot&, const TimeSlot&) noexcept`: half-open intervals; touching slots do not overlap.
- `std::vector<UtcSlot> merge_slots(std::span<const TimeSlot>)`: sorted, merged, UTC; overlapping or touching slots are combined.
- `std::chrono::milliseconds total_booked(std::span<const TimeSlot>)`: overlapped time counted once.

Edge cases:

- Slots in different zones compare by absolute instant, not by wall fields.
- Merging keeps the largest end when one slot contains another.
- Total booked time across a DST span within one zone is elapsed time (for example 23 h on the spring-forward day).
- Empty input gives zero and an empty result.

Concepts: `std::span` input, `noexcept` where it is right, sort-and-merge over intervals, value-type design for a non-trivial class.

## Part 7a: humanize

API (`humanize.hpp`):

- `std::string humanize(const ZonedInstant& when, const ZonedInstant& now)`

Rules:

- Within 45 seconds (either side) → `"just now"`.
- Minutes, hours, and days use the absolute difference. Units are singular for 1: `"in 1 minute"`, `"2 days ago"`.
- Older than 30 days → `"on YYYY-MM-DD"` in the zone of the instant, not of `now`.

Concepts: absolute vs signed differences, `std::format` with chrono specifiers (`{:%F}`), zone-aware formatting.

## Part 7b: Stopwatch over an injected steady clock

API (`stopwatch.hpp`):

- `template <SteadyClock Clock = std::chrono::steady_clock> class Stopwatch`
- `void start()`: `std::logic_error` if already running.
- `nanoseconds stop()`: returns elapsed since start and stops; `std::logic_error` if not running.
- `nanoseconds elapsed() const`: live while running, last value after `stop()`, zero before the first `start()`.

Edge cases:

- `system_clock` does not model `SteadyClock` (it is not steady); the concept test checks that at compile time.
- Tests inject a fake clock with a static `now_value`, so no real time is measured.

Concepts: concepts over clock types (`is_clock_v`, `is_steady`), zero-cost policy injection through a template parameter, why wall clocks must not time intervals.

## Concepts exercised (summary)

- `duration` arithmetic: `floor`, `round`, `duration_cast`, ratio types.
- `sys_time`, `local_time`, `zoned_time`, `time_zone`, `choose`, the tzdb.
- Calendar types: `year_month_day`, `year_month_day_last`, `weekday_indexed`, `months`.
- Parsing: `from_stream`, `from_chars`, strict validation.
- `std::format` for chrono types.
- Concepts for clock types; `noexcept`; value semantics.

## Run

On Windows, prefix CMake and ctest with `tools\vsenv.cmd` so MSVC is on the path:

```
tools\vsenv.cmd cmake --workflow --preset dev
tools\vsenv.cmd ctest --preset dev -R "17_chrono_time\.Part2"
tools\vsenv.cmd cmake --build --preset dev --target ex17_chrono_time_tests
tools\vsenv.cmd cmake --workflow --preset solutions
```

Each Part is a separate ctest filter (`17_chrono_time\.Part1` … `Part7`). Against the
stubs, every test fails.

Portability: the code targets C++23 and builds with MSVC 19.38, GCC 14, and Clang 18.
Clang 18's libc++ does not ship the tzdb, so the zone tests need a libc++ that provides
`std::chrono::get_tzdb()` (or use MSVC / libstdc++).

## Stretch ideas (untested)

- Accept `Europe/Berlin` vs `CET`-style abbreviations and reject ambiguous names.
- Add a `Gap::error` policy and an `Overlap::error` policy.
- Persist `WeeklyRule` as JSON and round-trip it through a fuzz test.
- Make `humanize` locale-aware via `std::format` with a user-supplied catalogue.
