// Engineer-owned. Contract: csv.hpp.
#include "csv.hpp"

#include <stdexcept>

namespace cppref::files {

std::string csv_escape(std::string_view) { throw std::logic_error("not implemented: csv_escape"); }

std::vector<std::vector<std::string>> csv_parse(std::string_view) {
    throw std::logic_error("not implemented: csv_parse");
}

CsvWriter::CsvWriter(std::ostream&) { throw std::logic_error("not implemented: CsvWriter::CsvWriter"); }

void CsvWriter::write_row(const std::vector<std::string>&) {
    throw std::logic_error("not implemented: CsvWriter::write_row");
}

void export_report(const Manifest&,
                   const std::filesystem::path&,
                   const std::map<std::string, std::string, std::less<>>&) {
    throw std::logic_error("not implemented: export_report");
}

std::vector<ReportRow> import_report(const std::filesystem::path&) {
    throw std::logic_error("not implemented: import_report");
}

}  // namespace cppref::files
