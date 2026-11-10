// Shared helpers for the *_test.cpp files (test-only; all functions inline).
#pragma once

#include "logging.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <mutex>
#include <random>
#include <source_location>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace cppref::logging::testing {

// In-memory sink: copies every record it receives.
class CaptureSink final : public Sink {
public:
    void write(const Record& record) override {
        std::scoped_lock lock(mutex_);
        records_.push_back(record);
    }

    void flush() override {
        flushes_.fetch_add(1);
    }

    std::vector<Record> records() const {
        std::scoped_lock lock(mutex_);
        return records_;
    }

    int flush_count() const noexcept {
        return flushes_.load();
    }

private:
    mutable std::mutex mutex_;
    std::vector<Record> records_;
    std::atomic<int> flushes_{0};
};

// Unique directory under the temp dir; removed on destruction.
class TempDir {
public:
    TempDir() : path_(make_unique_dir()) {}
    ~TempDir() {
        std::error_code ec;
        std::filesystem::remove_all(path_, ec);
    }
    TempDir(const TempDir&) = delete;
    TempDir& operator=(const TempDir&) = delete;

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

private:
    static std::filesystem::path make_unique_dir() {
        std::random_device rd;
        auto dir = std::filesystem::temp_directory_path() /
                   ("cppref-logging-" + std::to_string(rd()) + "-" + std::to_string(rd()));
        std::filesystem::create_directories(dir);
        return dir;
    }

    std::filesystem::path path_;
};

inline std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

inline std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

// A fixed instant: 2023-11-14T22:13:20.123Z.
inline std::chrono::system_clock::time_point fixed_time() {
    return std::chrono::system_clock::time_point{std::chrono::milliseconds{1'700'000'000'123}};
}

// Record with the given level, logger and message; `location` is the caller's.
inline Record make_record(Level level, std::string logger, std::string message,
                          std::vector<Field> fields = {}) {
    Record r;
    r.time = fixed_time();
    r.level = level;
    r.logger = std::move(logger);
    r.message = std::move(message);
    r.location = std::source_location::current();
    r.thread = std::this_thread::get_id();
    r.fields = std::move(fields);
    return r;
}

}  // namespace cppref::logging::testing
