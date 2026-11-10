# 18 · Build and deployment

## Scenario

You own the `cppref-service` binary that ships in a container. Before it goes
live it must:

- read its configuration from the environment and an optional `.env` file, and
  fail fast with every problem listed at once;
- expose a small CLI (`serve`, `check-config`, `version`) with predictable exit codes;
- stamp its version and git SHA into the binary at build time through CMake;
- shut down cleanly on SIGINT and SIGTERM;
- report readiness as JSON, with per-check timeouts so one hung dependency cannot block the probe;
- ship a Dockerfile and `.dockerignore` that pass a container lint (pinned images,
  multi-stage build, non-root user, exec-form entrypoint, health check).

Everything is implemented as plain C++23 with no daemon, no network, and no
third-party libraries beyond GoogleTest.

## Layout

| File | Owner | Purpose |
| --- | --- | --- |
| `config.hpp/.cpp` | you | Part 1: `Config::from_env`, `.env` parsing, masking |
| `cli.hpp/.cpp` | you | Part 2: `cli::run` argument handling and exit codes |
| `build_info.hpp/.cpp` | you | Part 3: `Version`, `parse_version`, `is_git_sha`, `build_info()` |
| `shutdown.hpp/.cpp` | you | Part 4: `SignalGuard` |
| `health.hpp/.cpp` | you | Part 5: `HealthRegistry`, `to_json` |
| `container_lint.hpp/.cpp` | you | Part 6: `lint_dockerfile`, `lint_dockerignore` |
| `Dockerfile`, `.dockerignore` | you | Part 6: the shipped container files |
| `app/main.cpp` | given | process entry point; identical in both trees |
| `CMakeLists.txt` | given | stamps `CPPREF_BUILD_VERSION` and `CPPREF_BUILD_GIT_SHA` |

The stubs throw `std::logic_error("not implemented: ...")`, so every test fails
until you implement the function behind it.

## Part 1 · Configuration from the environment

```cpp
using EnvLookup = std::function<std::optional<std::string>(std::string_view)>;
using DotenvMap = std::map<std::string, std::string, std::less<>>;

std::expected<bool, std::string>                  parse_bool(std::string_view);
std::expected<std::chrono::milliseconds, std::string> parse_duration(std::string_view);
std::vector<std::string>                          parse_list(std::string_view);
std::string                                       mask_url_password(std::string_view);
std::expected<DotenvMap, std::string>             load_dotenv(std::string_view);
EnvLookup                                         with_dotenv(EnvLookup, DotenvMap);
std::string                                       redacted(const Config&);

struct Config {
    static Config from_env(const EnvLookup&);   // throws ConfigError
    // database_url, api_token, app_name, port, debug, allowed_hosts,
    // request_timeout, log_level, git_sha
};
```

- `parse_bool`: `1 true yes on` / `0 false no off`, case-insensitive, whitespace stripped.
- `parse_duration`: `500ms`, `30s`, `5m`, `1h`, bare `2` (seconds), `1.5s`. Rejects empty, negative, unknown units, and prose.
- `parse_list`: comma-separated, items stripped, empty items dropped.
- `mask_url_password`: `postgresql://app:hunter2@db/x` becomes `postgresql://app:***@db/x`. The last `@` in the authority separates the password, so `p@ss` is handled.
- `load_dotenv`: `export KEY=value`, `#` comments, single quotes (literal), double quotes with `\n \t \" \\`, inline `#` after whitespace. Errors read `line N: ...`. A later duplicate wins.
- `with_dotenv`: the real environment always wins over the file. An environment variable set to an empty string shadows the file value.
- `from_env`: empty or whitespace-only values count as unset. It collects every problem and throws one `ConfigError` whose `what()` is `invalid configuration: VAR: problem; VAR: problem`. Secrets never appear in messages.

Rules: `DATABASE_URL` (required, `postgresql://` or `sqlite:`), `API_TOKEN` (required, at least 16 characters), `APP_NAME` (default `cppref-service`), `PORT` (1–65535, default 8080), `DEBUG` (boolean), `ALLOWED_HOSTS` (list), `REQUEST_TIMEOUT` (duration > 0, default 30s), `LOG_LEVEL` (default `INFO`), `GIT_SHA` (7–40 lowercase hex, normalised to lowercase).

## Part 2 · The command-line front end

```cpp
int run(std::span<const std::string_view> args, const EnvLookup& env,
        std::ostream& out, std::ostream& err);
```

`args` excludes `argv[0]`.

```
[--env-file PATH] serve [--host H] [--port P] [--dry-run]
[--env-file PATH] check-config
version
```

- `version` prints `cppref-service <version> (<sha>)` and exits 0. It reads no config.
- `check-config` prints `configuration ok` (exit 0), or one `error: <entry>` line per problem on stderr (exit 2).
- `serve --dry-run` prints `dry run: <app> would listen on <host>:<port>` (exit 0).
- `serve` without `--dry-run` installs a `SignalGuard` on SIGINT and SIGTERM, prints a ready line, and returns 0 when the signal arrives.
- Usage errors print `error: <message>` and the usage line to stderr and return 2. A missing command, an unknown command, an unknown option, a bad `--port`, an unreadable `--env-file`, or a malformed `.env` all return 2.
- `run` never throws for user mistakes.

## Part 3 · Build metadata

```cpp
struct Version { unsigned major, minor, patch; auto operator<=>(const Version&) const = default; std::string to_string() const; };
std::expected<Version, std::string> parse_version(std::string_view);   // strict X.Y.Z
bool is_git_sha(std::string_view);                                       // 7..40 lowercase hex
struct BuildInfo { std::string version; std::string git_sha; std::string version_line() const; };
BuildInfo build_info();                                                  // from compile definitions
```

- `parse_version` rejects `v1.2.3`, pre-release suffixes, leading zeros, missing or extra parts, and values that overflow `unsigned`.
- `build_info()` reads `CPPREF_BUILD_VERSION` and `CPPREF_BUILD_GIT_SHA`. CMake sets both. The SHA comes from `git rev-parse --short=12 HEAD` unless `CPPREF_BUILD_GIT_SHA` is passed, and falls back to `unknown`.

## Part 4 · Graceful shutdown

```cpp
class SignalGuard {
public:
    explicit SignalGuard(std::initializer_list<int> signals);  // logic_error if one is already alive
    ~SignalGuard();                                             // restores previous handlers
    std::stop_token token() const;
    std::optional<int> received_signal() const;
};
```

The signal handler is async-signal-safe: it performs one lock-free
compare-and-swap into a global atomic. A watcher `std::jthread` converts that
flag into `stop_source::request_stop()`. The first signal wins; later signals are
ignored. The destructor stops the watcher (last-declared member, joined first)
and restores the previous dispositions in reverse order.

## Part 5 · Readiness checks

```cpp
using CheckFn = std::function<std::expected<void, std::string>(std::stop_token)>;

class HealthRegistry {
public:
    void add(std::string name, CheckFn check, std::chrono::milliseconds timeout);  // dup or <= 0 -> invalid_argument
    std::size_t size() const;
    std::vector<CheckReport> run_all() const;   // registration order, never throws
};
bool all_ok(const std::vector<CheckReport>&);
std::string escape_json(std::string_view);
std::string to_json(const std::vector<CheckReport>&);
```

- A check that throws becomes a failure with detail `exception: <what()>`.
- A check that exceeds its timeout gets its stop token requested, and is reported as `timeout after <N>ms`. `run_all` still returns once the check honours the token, so a check that ignores the token blocks the probe; that is the documented contract.
- `to_json` emits `{"status":"ready"|"not_ready","checks":{"name":"ok"|"error: detail"}}` with full string escaping.

## Part 6 · Container files

```cpp
std::vector<LintIssue> lint_dockerfile(std::string_view);
std::vector<LintIssue> lint_dockerignore(std::string_view);
```

`LintIssue {line, message}`; `line == 0` means the whole file.

Dockerfile rules:

1. Every `FROM` pins a tag other than `latest`, or a digest (`scratch` is allowed).
2. At least two stages; the final stage uses `COPY --from`.
3. `COPY` and `ADD` never copy `.`.
4. A final `USER` exists and is not root or 0.
5. Every `EXPOSE` port is numeric (`/tcp` and `/udp` suffixes allowed).
6. `ENTRYPOINT` and `CMD` use exec (JSON array) form.
7. A `HEALTHCHECK` exists, is not `NONE`, and uses exec form for its `CMD`.

Continuation lines (`\`) join into one instruction whose line is its first physical line.

`.dockerignore` rules: must exclude `.git`, `build`, and `.env` (after stripping `./`, `**/`, and trailing `/`), and must not re-include them with `!`.

## Concepts exercised

- 12-factor configuration: injected `EnvLookup`, `std::expected` for parse results, aggregated exceptions (`ConfigError`).
- Parsing with `std::from_chars`, `std::string_view` slicing, transparent `std::less<>` lookups.
- `std::span` and exit-code discipline; streams passed in for testability.
- CMake compile definitions for build metadata; `find_package(Threads)`.
- RAII signal handling: `std::signal`, `std::atomic`, `std::jthread`, `std::stop_source`, `std::stop_token`.
- Concurrency with timeouts: `std::condition_variable::wait_for`, shared state via `std::make_shared` so an abandoned worker cannot dangle.
- JSON escaping by hand; `std::ranges` algorithms.
- Container hygiene as code: multi-stage builds, pinned bases, least privilege, exec-form entrypoints.

## Run

Windows (from the repository root, with the toolchain wrapper):

```
tools\vsenv.cmd cmake --workflow --preset dev
tools\vsenv.cmd ctest --preset dev -R "18_build_and_deployment\.Part2"
tools\vsenv.cmd cmake --build --preset dev --target ex18_build_and_deployment_tests
```

Solutions reference build:

```
tools\vsenv.cmd cmake --workflow --preset solutions
```

Stubs are expected to fail. Each test must report an exception from the
`not implemented` stub, never a hang or crash.

To exercise the process-level tests by hand, run the built CLI directly:

```
build\dev\exercises\18_build_and_deployment\ex18_cli.exe version
build\dev\exercises\18_build_and_deployment\ex18_cli.exe --env-file .env check-config
```

## Stretch ideas (not tested)

- Add a `--listen` mode backed by a minimal HTTP/1.1 server (POSIX sockets), and serve `/healthz` from `HealthRegistry`.
- Replace the watcher thread with `signalfd` on Linux, and compare the two designs.
- Extend `lint_dockerfile` with `ARG` secret detection (`ARG *TOKEN*`) and `apt-get` cleanup checks.
- Generate the `CPPREF_BUILD_*` values with `configure_file` instead of compile definitions.
