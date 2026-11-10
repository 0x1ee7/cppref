#include "logging.hpp"

#include <stdexcept>

// Stub note: functions marked noexcept return harmless values instead of
// throwing (a throw would std::terminate the test binary). Tests that depend on
// them fail on their assertions; everything else throws "not implemented".

namespace cppref::logging {

std::expected<Level, ParseLevelError> parse_level(std::string_view) {
    throw std::logic_error("not implemented: parse_level");
}

std::string_view to_string(Level) noexcept {
    return {};
}

std::string format_utc_iso8601(std::chrono::system_clock::time_point) {
    throw std::logic_error("not implemented: format_utc_iso8601");
}

MdcScope::MdcScope(std::string_view, std::string_view) {
    throw std::logic_error("not implemented: MdcScope");
}

MdcScope::~MdcScope() = default;

std::vector<Field> mdc_snapshot() {
    throw std::logic_error("not implemented: mdc_snapshot");
}

Logger::Logger(std::string, Logger*) {
    throw std::logic_error("not implemented: Logger");
}

std::string_view Logger::name() const noexcept {
    return {};
}

Logger* Logger::parent() const noexcept {
    return nullptr;
}

std::optional<Level> Logger::level() const noexcept {
    return std::nullopt;
}

void Logger::set_level(Level) noexcept {}

void Logger::clear_level() noexcept {}

Level Logger::effective_level() const noexcept {
    return Level::off;
}

bool Logger::enabled(Level) const noexcept {
    return false;
}

bool Logger::propagate() const noexcept {
    return false;
}

void Logger::set_propagate(bool) noexcept {}

void Logger::add_sink(std::shared_ptr<Sink>) {
    throw std::logic_error("not implemented: Logger::add_sink");
}

void Logger::clear_sinks() noexcept {}

void Logger::emit(Level, std::source_location, std::string) {
    throw std::logic_error("not implemented: Logger::emit");
}

Registry::Registry() {
    throw std::logic_error("not implemented: Registry");
}

Registry::~Registry() = default;

Logger& Registry::root() {
    throw std::logic_error("not implemented: Registry::root");
}

Logger& Registry::get(std::string_view) {
    throw std::logic_error("not implemented: Registry::get");
}

}  // namespace cppref::logging
