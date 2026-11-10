#include "logging.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

namespace cppref::logging {

namespace {

// Per-thread MDC stack. Scopes push on construction and pop on destruction;
// LIFO discipline (guaranteed by RAII) keeps this a plain vector.
struct MdcEntry {
    std::string key;
    std::string value;
};

thread_local std::vector<MdcEntry> t_mdc;

char ascii_lower(char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

}  // namespace

std::expected<Level, ParseLevelError> parse_level(std::string_view text) {
    std::string lower(text.size(), '\0');
    for (std::size_t i = 0; i < text.size(); ++i) {
        lower[i] = ascii_lower(text[i]);
    }
    if (lower == "trace") return Level::trace;
    if (lower == "debug") return Level::debug;
    if (lower == "info") return Level::info;
    if (lower == "warn" || lower == "warning") return Level::warn;
    if (lower == "error") return Level::error;
    if (lower == "off") return Level::off;
    return std::unexpected(ParseLevelError{std::string(text)});
}

std::string_view to_string(Level level) noexcept {
    switch (level) {
        case Level::trace: return "TRACE";
        case Level::debug: return "DEBUG";
        case Level::info: return "INFO";
        case Level::warn: return "WARN";
        case Level::error: return "ERROR";
        case Level::off: return "OFF";
    }
    return "OFF";
}

std::string format_utc_iso8601(std::chrono::system_clock::time_point tp) {
    using namespace std::chrono;
    const auto secs = floor<seconds>(tp);
    const auto millis = duration_cast<milliseconds>(tp - secs).count();
    const auto day_point = floor<days>(secs);
    const year_month_day ymd{day_point};
    const hh_mm_ss hms{secs - day_point};
    return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:03}Z",
                       static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month()),
                       static_cast<unsigned>(ymd.day()), hms.hours().count(),
                       hms.minutes().count(), hms.seconds().count(), millis);
}

MdcScope::MdcScope(std::string_view key, std::string_view value) {
    t_mdc.push_back(MdcEntry{std::string(key), std::string(value)});
}

MdcScope::~MdcScope() {
    t_mdc.pop_back();
}

std::vector<Field> mdc_snapshot() {
    std::vector<Field> out;
    for (const auto& entry : t_mdc) {
        auto it = std::find_if(out.begin(), out.end(),
                               [&](const Field& f) { return f.key == entry.key; });
        if (it != out.end()) {
            it->value = entry.value;  // inner scope shadows the outer value
        } else {
            out.push_back(Field{entry.key, entry.value});
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Logger
// ---------------------------------------------------------------------------

Logger::Logger(std::string name, Logger* parent) : name_(std::move(name)), parent_(parent) {}

std::string_view Logger::name() const noexcept {
    return name_;
}

Logger* Logger::parent() const noexcept {
    return parent_;
}

std::optional<Level> Logger::level() const noexcept {
    const int value = level_.load();
    if (value == kInherit) return std::nullopt;
    return static_cast<Level>(value);
}

void Logger::set_level(Level level) noexcept {
    level_.store(static_cast<int>(level));
}

void Logger::clear_level() noexcept {
    level_.store(kInherit);
}

Level Logger::effective_level() const noexcept {
    for (const Logger* l = this; l != nullptr; l = l->parent_) {
        const int value = l->level_.load();
        if (value != kInherit) return static_cast<Level>(value);
    }
    return Level::info;  // unreachable: the root always has an explicit level
}

bool Logger::enabled(Level level) const noexcept {
    return level != Level::off && level >= effective_level();
}

bool Logger::propagate() const noexcept {
    return propagate_.load();
}

void Logger::set_propagate(bool propagate) noexcept {
    propagate_.store(propagate);
}

void Logger::add_sink(std::shared_ptr<Sink> sink) {
    std::scoped_lock lock(sinks_mutex_);
    sinks_.push_back(std::move(sink));
}

void Logger::clear_sinks() noexcept {
    std::scoped_lock lock(sinks_mutex_);
    sinks_.clear();
}

void Logger::emit(Level level, std::source_location location, std::string message) {
    Record record;
    record.time = std::chrono::system_clock::now();
    record.level = level;
    record.logger = name_;
    record.message = std::move(message);
    record.location = location;
    record.thread = std::this_thread::get_id();
    record.fields = mdc_snapshot();

    for (const Logger* l = this; l != nullptr; l = l->parent_) {
        // Copy the sink list so the lock is not held while sinks run; a sink
        // that logs again must not deadlock on this logger.
        std::vector<std::shared_ptr<Sink>> sinks;
        {
            std::scoped_lock lock(l->sinks_mutex_);
            sinks = l->sinks_;
        }
        for (const auto& sink : sinks) {
            sink->write(record);
        }
        if (!l->propagate_.load()) break;
    }
}

// ---------------------------------------------------------------------------
// Registry
// ---------------------------------------------------------------------------

Registry::Registry() : root_(new Logger("", nullptr)) {
    root_->set_level(Level::info);
}

Registry::~Registry() = default;

Logger& Registry::root() {
    return *root_;
}

Logger& Registry::get(std::string_view name) {
    if (name.empty()) {
        throw std::invalid_argument("logger name must not be empty");
    }
    // Validate every segment before touching the map.
    for (std::size_t start = 0;;) {
        const std::size_t dot = name.find('.', start);
        const std::size_t end = dot == std::string_view::npos ? name.size() : dot;
        if (end == start) {
            throw std::invalid_argument("logger name has an empty segment: " + std::string(name));
        }
        if (dot == std::string_view::npos) break;
        start = dot + 1;
    }

    std::scoped_lock lock(mutex_);
    Logger* parent = root_.get();
    for (std::size_t start = 0;;) {
        const std::size_t dot = name.find('.', start);
        const std::string_view prefix = name.substr(0, dot);
        auto it = loggers_.find(prefix);
        if (it == loggers_.end()) {
            std::unique_ptr<Logger> created(new Logger(std::string(prefix), parent));
            it = loggers_.emplace(std::string(prefix), std::move(created)).first;
        }
        parent = it->second.get();
        if (dot == std::string_view::npos) break;
        start = dot + 1;
    }
    return *parent;
}

}  // namespace cppref::logging
