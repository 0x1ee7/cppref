// RFC 4180 CSV reading/writing and the sync tool's report format.
#pragma once

#include "scan.hpp"

#include <array>
#include <filesystem>
#include <iosfwd>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::files {

// Malformed CSV, or a report file that does not match the expected layout. The message names the line.
class CsvError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Wraps `field` in double quotes (doubling inner quotes) only when it contains ',', '"', CR or LF.
[[nodiscard]] std::string csv_escape(std::string_view field);

// Parses RFC 4180 text. Records end in CRLF or LF; a final terminator does not create an
// extra record; empty input yields no records; an empty line in the middle yields one record
// holding one empty field. Quoted fields may contain separators, doubled quotes and newlines.
// A quote inside an unquoted field, text after a closing quote, or an unterminated quoted
// field -> CsvError.
[[nodiscard]] std::vector<std::vector<std::string>> csv_parse(std::string_view text);

// Writes rows to a caller-owned stream; each row is escaped and terminated by CRLF.
class CsvWriter {
public:
    explicit CsvWriter(std::ostream& out);
    void write_row(const std::vector<std::string>& fields);

    // engineer: std::ostream& out_
};

// Column layout of the report, in order.
inline constexpr std::array<std::string_view, 5> report_fields{"path", "size", "mtime", "sha256", "note"};

struct ReportRow {
    std::string path;
    std::string size;
    std::string mtime;
    std::string sha256;
    std::string note;
    friend bool operator==(const ReportRow&, const ReportRow&) = default;
};

// Writes the report atomically (write_file_atomic): UTF-8, no BOM, CRLF rows, header first, then one
// row per manifest entry in path order.
//   size   : decimal byte count
//   mtime  : "YYYY-MM-DDTHH:MM:SS.ffffffZ" (UTC, six fractional digits)
//   note   : notes[path], or "" when absent
// A key in `notes` that is not in `manifest` -> std::invalid_argument (checked before writing).
void export_report(const Manifest& manifest,
                   const std::filesystem::path& path,
                   const std::map<std::string, std::string, std::less<>>& notes = {});

// Header must equal report_fields exactly and every data row must have 5 fields; otherwise
// CsvError. All values are returned as strings, in file order.
[[nodiscard]] std::vector<ReportRow> import_report(const std::filesystem::path& path);

}  // namespace cppref::files
