// Concrete sinks and formatters: text, JSON lines, ostream, size-rotated file.
#pragma once

#include "logging.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iosfwd>
#include <mutex>
#include <string>
#include <string_view>

namespace cppref::logging {

using Formatter = std::function<std::string(const Record&)>;

// Human-readable line, without trailing newline:
//   "2023-11-14T22:13:20.123Z INFO  app.db: message k=v k2=v2"
// Level is padded to five columns; MDC fields are appended as " key=value".
std::string text_format(const Record& record);

// One JSON object, without trailing newline:
//   {"ts":"...","level":"info","logger":"app.db","msg":"...",
//    "file":"...","line":12,"fields":{"k":"v"}}
// `level` is lower-case here. Every string is escaped with json_escape; the
// object is always a single line even when the message contains newlines.
std::string json_format(const Record& record);

// Escapes text for the body of a JSON string literal (quotes not included):
// '"' and '\\', the short escapes \b \f \n \r \t, other control characters
// below 0x20 as \u00XX. Bytes >= 0x80 pass through unchanged (UTF-8).
std::string json_escape(std::string_view text);

// Writes one formatted line per record to a caller-owned std::ostream.
// The stream must outlive the sink. write() and flush() serialise on an
// internal mutex, so lines never interleave across threads.
class OstreamSink final : public Sink {
public:
    explicit OstreamSink(std::ostream& out, Formatter formatter = text_format);

    void write(const Record& record) override;
    void flush() override;

private:
    std::ostream& out_;
    Formatter formatter_;
    std::mutex mutex_;
};

// Size-based rotating file. Records are appended to `path`. Before a line is
// written, if the current file is non-empty and would exceed `max_bytes`, the
// file is rotated:
//   path.(max_files-1) -> path.max_files   (the old path.max_files is deleted)
//   ...
//   path.1 -> path.2
//   path   -> path.1
// and a fresh `path` is started. At most `max_files` backups are kept. A
// single line larger than `max_bytes` is written to a fresh file rather than
// dropped. An existing `path` is appended to and its size counts toward the
// limit. The file is opened in the constructor.
//
// Preconditions (std::invalid_argument): max_bytes > 0, max_files >= 1.
// Open failures throw std::system_error.
class RotatingFileSink final : public Sink {
public:
    RotatingFileSink(std::filesystem::path path, std::uintmax_t max_bytes,
                     std::size_t max_files, Formatter formatter = text_format);
    ~RotatingFileSink() override;

    RotatingFileSink(const RotatingFileSink&) = delete;
    RotatingFileSink& operator=(const RotatingFileSink&) = delete;

    void write(const Record& record) override;
    void flush() override;

    const std::filesystem::path& path() const noexcept;

private:
    void open_for_append();
    void rotate_locked();
    std::filesystem::path backup_path(std::size_t n) const;

    std::filesystem::path path_;
    std::uintmax_t max_bytes_;
    std::size_t max_files_;
    Formatter formatter_;
    std::ofstream out_;
    std::uintmax_t size_ = 0;
    std::mutex mutex_;
};

}  // namespace cppref::logging
