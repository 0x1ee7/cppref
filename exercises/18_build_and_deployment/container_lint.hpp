// Part 6 · Container files: lint a Dockerfile and .dockerignore without a Docker daemon.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::deployment::container {

struct LintIssue {
    std::size_t line = 0;   // 1-based line of the offending instruction; 0 = whole file
    std::string message;

    friend bool operator==(const LintIssue&, const LintIssue&) = default;
};

/// Dockerfile rules (see README, Part 6 for the full list):
///  - every FROM pins a tag (not latest, not untagged) or a digest; `scratch` is allowed;
///  - multi-stage: at least two FROMs, and the final stage uses COPY --from;
///  - COPY/ADD never copy "." (the whole context);
///  - a final USER exists and is not root/0;
///  - EXPOSE names numeric ports; ENTRYPOINT and CMD use exec (JSON array) form;
///  - HEALTHCHECK exists, is not NONE, and its CMD uses exec form.
/// Continuation lines (`\`) join into one instruction whose line is the first physical line.
/// Comments (`#`) and blank lines are ignored.
[[nodiscard]] std::vector<LintIssue> lint_dockerfile(std::string_view text);

/// .dockerignore rules: must exclude `.git`, `build`, and `.env` (after stripping
/// leading "./" and "**/" and a trailing "/"); must not re-include them with `!`.
[[nodiscard]] std::vector<LintIssue> lint_dockerignore(std::string_view text);

}  // namespace cppref::deployment::container
