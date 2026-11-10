#include "errors.hpp"

#include <stdexcept>

namespace cppref::error_handling {

SettingMissing::SettingMissing(std::string /*key*/) : ConfigError("") {
    throw std::logic_error("not implemented: SettingMissing::SettingMissing");
}

const std::string& SettingMissing::key() const {
    throw std::logic_error("not implemented: SettingMissing::key");
}

std::vector<std::string> describe_chain(const std::exception& /*e*/) {
    throw std::logic_error("not implemented: describe_chain");
}

}  // namespace cppref::error_handling
