// RFC 4180 CSV parsing and writing, and the backup report built on it.
#pragma once

#include "scan.hpp"

#include <array>
#include <filesystem>
#include <map>
#include <ostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::files {

class CsvError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// RFC 4180 quoting: a field is wrapped in double quotes when it contains ',', '"', CR or
// LF; embedded quotes are doubled. Other fields are returned unchanged (including empty).
[[nodiscard]] std::string csv_escape(std::string_view field);

// Parses text into rows of fields. Records end with CRLF or LF; a final record may omit
// the terminator. Empty input yields no rows. Quoted fields may contain commas, quotes
// (as ""), and newlines. Malformed input (unterminated quote, text after a closing quote,
// quote inside an unquoted field) -> CsvError.
[[nodiscard]] std::vector<std::vector<std::string>> csv_parse(std::string_view text);

// Writes rows terminated by CRLF, escaping each field with csv_escape.
class CsvWriter {
public:
    explicit CsvWriter(std::ostream& out) : out_(out) {}
    void write_row(const std::vector<std::string>& fields);

private:
    std::ostream& out_;
};

// Report columns, in order: path, size, mtime, sha256, note.
inline constexpr std::array<std::string_view, 5> report_fields = {"path", "size", "mtime", "sha256", "note"};

// One report row. mtime is text: "YYYY-MM-DDTHH:MM:SS.ffffffZ" (UTC, microseconds).
struct ReportRow {
    std::string path;
    std::string size;
    std::string mtime;
    std::string sha256;
    std::string note;
    friend bool operator==(const ReportRow&, const ReportRow&) = default;
};

// Writes a CSV report with a header row and one row per manifest entry, sorted by key.
// `notes` maps a key to an optional note; keys without a note get an empty field.
// A note whose key is absent from the manifest -> std::invalid_argument.
// The file is written via write_file_atomic.
void export_report(const Manifest& manifest, const std::filesystem::path& path,
                   const std::map<std::string, std::string, std::less<>>& notes = {});

// Reads a report written by export_report. The header must equal report_fields exactly.
// Missing header, wrong header, or any row whose field count is not 5 -> CsvError.
[[nodiscard]] std::vector<ReportRow> import_report(const std::filesystem::path& path);

}  // namespace cppref::files
