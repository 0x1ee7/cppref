#include "csv.hpp"

#include "file_io.hpp"

#include <chrono>
#include <cstdio>
#include <sstream>
#include <utility>

namespace cppref::files {
namespace fs = std::filesystem;

namespace {

std::string format_mtime(Timestamp t) {
    using namespace std::chrono;
    const auto secs = floor<seconds>(t);
    const auto micros = static_cast<long long>((t - secs).count());
    const sys_days day = floor<days>(secs);
    const year_month_day ymd{day};
    const hh_mm_ss<seconds> hms{secs - day};

    char buf[64];
    std::snprintf(buf, sizeof buf, "%04d-%02u-%02uT%02d:%02d:%02dZ",
                  int(ymd.year()), unsigned(ymd.month()), unsigned(ymd.day()),
                  int(hms.hours().count()), int(hms.minutes().count()), int(hms.seconds().count()));
    std::string out(buf);
    // Insert the fractional part before the trailing 'Z'.
    char frac[16];
    std::snprintf(frac, sizeof frac, ".%06lld", micros);
    out.insert(out.size() - 1, frac);
    return out;
}

}  // namespace

std::string csv_escape(std::string_view field) {
    const bool needs_quotes = field.find_first_of(",\"\r\n") != std::string_view::npos;
    if (!needs_quotes) {
        return std::string(field);
    }
    std::string out;
    out.reserve(field.size() + 2);
    out += '"';
    for (const char c : field) {
        if (c == '"') {
            out += '"';
        }
        out += c;
    }
    out += '"';
    return out;
}

std::vector<std::vector<std::string>> csv_parse(std::string_view text) {
    std::vector<std::vector<std::string>> rows;
    std::size_t pos = 0;
    const std::size_t n = text.size();
    while (pos < n) {
        std::vector<std::string> row;
        for (;;) {
            std::string field;
            if (pos < n && text[pos] == '"') {
                ++pos;
                bool closed = false;
                while (pos < n) {
                    if (text[pos] == '"') {
                        if (pos + 1 < n && text[pos + 1] == '"') {
                            field += '"';
                            pos += 2;
                        } else {
                            ++pos;
                            closed = true;
                            break;
                        }
                    } else {
                        field += text[pos++];
                    }
                }
                if (!closed) {
                    throw CsvError("unterminated quoted field");
                }
                if (pos < n && text[pos] != ',' && text[pos] != '\r' && text[pos] != '\n') {
                    throw CsvError("unexpected text after closing quote");
                }
            } else {
                while (pos < n && text[pos] != ',' && text[pos] != '\r' && text[pos] != '\n') {
                    if (text[pos] == '"') {
                        throw CsvError("quote inside unquoted field");
                    }
                    field += text[pos++];
                }
            }
            row.push_back(std::move(field));
            if (pos < n && text[pos] == ',') {
                ++pos;
                continue;
            }
            break;
        }
        if (pos < n && text[pos] == '\r') {
            ++pos;
            if (pos < n && text[pos] == '\n') {
                ++pos;
            }
        } else if (pos < n && text[pos] == '\n') {
            ++pos;
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

void CsvWriter::write_row(const std::vector<std::string>& fields) {
    for (std::size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) {
            out_ << ',';
        }
        out_ << csv_escape(fields[i]);
    }
    out_ << "\r\n";
}

void export_report(const Manifest& manifest, const fs::path& path,
                   const std::map<std::string, std::string, std::less<>>& notes) {
    for (const auto& note : notes) {
        if (!manifest.contains(note.first)) {
            throw std::invalid_argument("note for key not in manifest: " + note.first);
        }
    }

    std::ostringstream out;
    CsvWriter writer(out);
    writer.write_row(std::vector<std::string>(report_fields.begin(), report_fields.end()));
    for (const auto& [key, entry] : manifest) {
        const auto note = notes.find(key);
        writer.write_row({
            key,
            std::to_string(entry.size),
            format_mtime(entry.mtime),
            entry.sha256,
            note == notes.end() ? std::string{} : note->second,
        });
    }
    write_file_atomic(path, out.str());
}

std::vector<ReportRow> import_report(const fs::path& path) {
    const auto rows = csv_parse(read_file_binary(path));
    if (rows.empty()) {
        throw CsvError("report is empty: missing header");
    }
    const std::vector<std::string> header(report_fields.begin(), report_fields.end());
    if (rows.front() != header) {
        throw CsvError("unexpected report header");
    }

    std::vector<ReportRow> out;
    for (std::size_t i = 1; i < rows.size(); ++i) {
        const auto& f = rows[i];
        if (f.size() != report_fields.size()) {
            throw CsvError("row " + std::to_string(i + 1) + ": expected 5 fields, got " +
                           std::to_string(f.size()));
        }
        out.push_back(ReportRow{f[0], f[1], f[2], f[3], f[4]});
    }
    return out;
}

}  // namespace cppref::files
