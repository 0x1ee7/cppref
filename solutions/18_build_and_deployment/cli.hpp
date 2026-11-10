// Part 2 · Command-line front end: argument parsing, exit codes, output discipline.
#pragma once

#include "config.hpp"

#include <ostream>
#include <span>
#include <string_view>

namespace cppref::deployment::cli {

/// Runs one invocation. `args` excludes argv[0].
///
///   [--env-file PATH] serve [--host H] [--port P] [--dry-run]
///   [--env-file PATH] check-config
///   version
///
/// Returns 0 on success; 2 on usage errors, unreadable or malformed .env files,
/// and configuration errors. Writes results to `out` and diagnostics to `err`.
/// Never throws for user mistakes.
int run(std::span<const std::string_view> args, const EnvLookup& env, std::ostream& out, std::ostream& err);

}  // namespace cppref::deployment::cli
