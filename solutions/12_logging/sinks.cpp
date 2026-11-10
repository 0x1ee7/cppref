#include "sinks.hpp"

#include <cstddef>
#include <format>
#include <ostream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace cppref::logging {

namespace {

namespace fs = std::filesystem;

std::string lower_level_name(Level level) {
    std::string name(to_string(level));
    for (char& c : name) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return name;
}

void rename_checked(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    fs::rename(from, to, ec);
    if (ec) {
        throw std::system_error(ec, "log rotation failed: " + from.string());
    }
}

}  // namespace

std::string text_format(const Record& record) {
    std::string out = format_utc_iso8601(record.time);
    out += std::format(" {:<5} {}: {}", to_string(record.level), record.logger, record.message);
    for (const auto& field : record.fields) {
        out += ' ';
        out += field.key;
        out += '=';
        out += field.value;
    }
    return out;
}

std::string json_escape(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (unsigned char c : text) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    out += std::format("\\u{:04x}", c);
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

std::string json_format(const Record& record) {
    std::string out;
    out += "{\"ts\":\"";
    out += format_utc_iso8601(record.time);
    out += "\",\"level\":\"";
    out += lower_level_name(record.level);
    out += "\",\"logger\":\"";
    out += json_escape(record.logger);
    out += "\",\"msg\":\"";
    out += json_escape(record.message);
    out += "\",\"file\":\"";
    out += json_escape(record.location.file_name());
    out += "\",\"line\":";
    out += std::to_string(record.location.line());
    out += ",\"fields\":{";
    bool first = true;
    for (const auto& field : record.fields) {
        if (!first) out += ',';
        first = false;
        out += '"';
        out += json_escape(field.key);
        out += "\":\"";
        out += json_escape(field.value);
        out += '"';
    }
    out += "}}";
    return out;
}

// ---------------------------------------------------------------------------
// OstreamSink
// ---------------------------------------------------------------------------

OstreamSink::OstreamSink(std::ostream& out, Formatter formatter)
    : out_(out), formatter_(std::move(formatter)) {}

void OstreamSink::write(const Record& record) {
    // Format outside the lock; only the actual stream write is serialised.
    std::string line = formatter_(record);
    line.push_back('\n');
    std::scoped_lock lock(mutex_);
    out_ << line;
}

void OstreamSink::flush() {
    std::scoped_lock lock(mutex_);
    out_.flush();
}

// ---------------------------------------------------------------------------
// RotatingFileSink
// ---------------------------------------------------------------------------

RotatingFileSink::RotatingFileSink(fs::path path, std::uintmax_t max_bytes, std::size_t max_files,
                                   Formatter formatter)
    : path_(std::move(path)),
      max_bytes_(max_bytes),
      max_files_(max_files),
      formatter_(std::move(formatter)) {
    if (max_bytes_ == 0) {
        throw std::invalid_argument("RotatingFileSink: max_bytes must be > 0");
    }
    if (max_files_ == 0) {
        throw std::invalid_argument("RotatingFileSink: max_files must be >= 1");
    }
    open_for_append();
    std::error_code ec;
    const auto existing = fs::file_size(path_, ec);
    size_ = ec ? 0 : existing;
}

RotatingFileSink::~RotatingFileSink() = default;

void RotatingFileSink::open_for_append() {
    out_.open(path_, std::ios::binary | std::ios::app);
    if (!out_) {
        throw std::system_error(std::make_error_code(std::errc::io_error),
                                "cannot open log file: " + path_.string());
    }
}

fs::path RotatingFileSink::backup_path(std::size_t n) const {
    fs::path backup = path_;
    backup += "." + std::to_string(n);
    return backup;
}

void RotatingFileSink::rotate_locked() {
    out_.close();
    // Drop the oldest backup, then shift the rest up by one.
    std::error_code ec;
    fs::remove(backup_path(max_files_), ec);
    for (std::size_t n = max_files_; n > 1; --n) {
        const fs::path from = backup_path(n - 1);
        if (fs::exists(from)) rename_checked(from, backup_path(n));
    }
    if (fs::exists(path_)) rename_checked(path_, backup_path(1));
    open_for_append();
    size_ = 0;
}

void RotatingFileSink::write(const Record& record) {
    std::string line = formatter_(record);
    line.push_back('\n');
    const auto added = static_cast<std::uintmax_t>(line.size());

    std::scoped_lock lock(mutex_);
    // A non-empty file is rotated before a line that would overflow it; an
    // empty file always accepts the line, so oversized lines are never dropped.
    if (size_ > 0 && size_ + added > max_bytes_) {
        rotate_locked();
    }
    out_.write(line.data(), static_cast<std::streamsize>(line.size()));
    size_ += added;
}

void RotatingFileSink::flush() {
    std::scoped_lock lock(mutex_);
    out_.flush();
}

const fs::path& RotatingFileSink::path() const noexcept {
    return path_;
}

}  // namespace cppref::logging
