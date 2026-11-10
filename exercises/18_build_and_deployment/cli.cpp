#include "cli.hpp"

#include <stdexcept>

namespace cppref::deployment::cli {

int run(std::span<const std::string_view>, const EnvLookup&, std::ostream&, std::ostream&) {
    throw std::logic_error("not implemented: cli::run");
}

}  // namespace cppref::deployment::cli
