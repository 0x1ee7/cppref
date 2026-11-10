// Part 6: RFC 4180 CSV and the sync report (csv.hpp).
#include "csv.hpp"
#include "test_support_test.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using cppref::files::CsvError;
using cppref::files::CsvWriter;
using cppref::files::FileEntry;
using cppref::files::Manifest;
using cppref::files::ReportRow;
using cppref::files::csv_escape;
using cppref::files::csv_parse;
using cppref::files::export_report;
using cppref::files::import_report;
using cppref::files::testing::TempDir;
using cppref::files::testing::read_bytes;
using cppref::files::testing::write_bytes;

using Rows = std::vector<std::vector<std::string>>;

TEST(Part6Escape, QuotesOnlyWhenNeeded) {
    EXPECT_EQ(csv_escape("plain"), "plain");
    EXPECT_EQ(csv_escape("a,b"), "\"a,b\"");
    EXPECT_EQ(csv_escape("say \"hi\""), "\"say \"\"hi\"\"\"");
    EXPECT_EQ(csv_escape("line1\nline2"), "\"line1\nline2\"");
    EXPECT_EQ(csv_escape(""), "");
}

TEST(Part6Parse, ParsesSimpleRecords) {
    EXPECT_EQ(csv_parse("a,b,c\r\n1,2,3\r\n"), (Rows{{"a", "b", "c"}, {"1", "2", "3"}}));
}

TEST(Part6Parse, AcceptsLfTerminatorsAndNoTrailingNewline) {
    EXPECT_EQ(csv_parse("a,b\n1,2"), (Rows{{"a", "b"}, {"1", "2"}}));
}

TEST(Part6Parse, QuotedFieldsMayContainSeparatorsQuotesAndNewlines) {
    EXPECT_EQ(csv_parse("\"x,y\",\"say \"\"hi\"\"\",\"two\nlines\"\r\n"),
              (Rows{{"x,y", "say \"hi\"", "two\nlines"}}));
}

TEST(Part6Parse, EmptyInputHasNoRecords) {
    EXPECT_TRUE(csv_parse("").empty());
}

TEST(Part6Parse, EmptyMiddleLineIsOneEmptyField) {
    EXPECT_EQ(csv_parse("a\r\n\r\nb\r\n"), (Rows{{"a"}, {""}, {"b"}}));
}

TEST(Part6Parse, RejectsStrayQuoteAndUnterminatedQuotedField) {
    EXPECT_THROW(static_cast<void>(csv_parse("ab\"c,d\r\n")), CsvError);
    EXPECT_THROW(static_cast<void>(csv_parse("\"abc,def\r\n")), CsvError);
    EXPECT_THROW(static_cast<void>(csv_parse("\"ok\"junk,x\r\n")), CsvError);
}

TEST(Part6Writer, WritesEscapedCrlfRows) {
    std::ostringstream out;
    CsvWriter writer(out);
    writer.write_row({"name", "note"});
    writer.write_row({"a,b", "say \"hi\""});
    EXPECT_EQ(out.str(), "name,note\r\n\"a,b\",\"say \"\"hi\"\"\"\r\n");
}

TEST(Part6Report, ExportWritesHeaderAndIsoMicrosecondMtime) {
    TempDir dir;
    // 2024-02-29T12:34:56.000123Z
    const auto mtime = cppref::files::Timestamp{std::chrono::microseconds{1'709'210'096'000'123}};
    Manifest m{{"a.txt", FileEntry{5, mtime, std::string(64, 'f')}}};
    export_report(m, dir.path() / "report.csv");
    EXPECT_EQ(read_bytes(dir.path() / "report.csv"),
              "path,size,mtime,sha256,note\r\n"
              "a.txt,5,2024-02-29T12:34:56.000123Z," + std::string(64, 'f') + ",\r\n");
}

TEST(Part6Report, ExportThenImportRoundTripsWithNotes) {
    TempDir dir;
    const auto mtime = cppref::files::Timestamp{std::chrono::microseconds{1'709'210'096'000'123}};
    Manifest m{
        {"a,comma.txt", FileEntry{1, mtime, std::string(64, '1')}},
        {"b.txt", FileEntry{2, mtime, std::string(64, '2')}},
    };
    export_report(m, dir.path() / "report.csv", {{"b.txt", "reviewed"}});

    const std::vector<ReportRow> rows = import_report(dir.path() / "report.csv");
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0].path, "a,comma.txt");
    EXPECT_EQ(rows[0].size, "1");
    EXPECT_EQ(rows[0].note, "");
    EXPECT_EQ(rows[1].path, "b.txt");
    EXPECT_EQ(rows[1].sha256, std::string(64, '2'));
    EXPECT_EQ(rows[1].note, "reviewed");
}

TEST(Part6Report, ExportRejectsNoteForUnknownKeyWithoutWriting) {
    TempDir dir;
    Manifest m{{"a.txt", FileEntry{1, {}, std::string(64, '0')}}};
    EXPECT_THROW(export_report(m, dir.path() / "report.csv", {{"ghost.txt", "x"}}), std::invalid_argument);
    EXPECT_FALSE(std::filesystem::exists(dir.path() / "report.csv"));
}

TEST(Part6Report, ImportRejectsWrongHeader) {
    TempDir dir;
    write_bytes(dir.path() / "bad.csv", "path,size,mtime,sha256\r\na,1,x,y\r\n");
    EXPECT_THROW(static_cast<void>(import_report(dir.path() / "bad.csv")), CsvError);
}

TEST(Part6Report, ImportRejectsRowsWithWrongFieldCount) {
    TempDir dir;
    write_bytes(dir.path() / "bad.csv", "path,size,mtime,sha256,note\r\na,1,x,y\r\n");
    EXPECT_THROW(static_cast<void>(import_report(dir.path() / "bad.csv")), CsvError);
}
