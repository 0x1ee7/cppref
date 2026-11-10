#include "errors.hpp"

#include <exception>
#include <string>
#include <utility>
#include <vector>

namespace cppref::error_handling {

namespace {

// The exception nested under `e`, or a null pointer when there is none.
std::exception_ptr nested_of(const std::exception& e) {
    const auto* nested = dynamic_cast<const std::nested_exception*>(&e);
    return nested != nullptr ? nested->nested_ptr() : nullptr;
}

}  // namespace

SettingMissing::SettingMissing(std::string key)
    : ConfigError("setting '" + key + "' is not defined"), key_(std::move(key)) {}

const std::string& SettingMissing::key() const {
    return key_;
}

std::vector<std::string> describe_chain(const std::exception& e) {
    std::vector<std::string> out{e.what()};
    std::exception_ptr next = nested_of(e);
    while (next) {
        try {
            std::rethrow_exception(next);
        } catch (const std::exception& inner) {
            out.emplace_back(inner.what());
            // Read the link before the catch block ends and `inner` is destroyed.
            next = nested_of(inner);
        } catch (...) {
            out.emplace_back("unknown exception");
            break;
        }
    }
    return out;
}

}  // namespace cppref::error_handling
