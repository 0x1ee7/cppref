#include "container_lint.hpp"

#include <stdexcept>

namespace cppref::deployment::container {

std::vector<LintIssue> lint_dockerfile(std::string_view) {
    throw std::logic_error("not implemented: lint_dockerfile");
}

std::vector<LintIssue> lint_dockerignore(std::string_view) {
    throw std::logic_error("not implemented: lint_dockerignore");
}

}  // namespace cppref::deployment::container
