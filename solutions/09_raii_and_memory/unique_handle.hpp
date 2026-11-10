#pragma once

#include <cstdio>
#include <expected>
#include <filesystem>
#include <system_error>
#include <utility>

namespace cppref::raii_and_memory {

// Move-only owner of a C-style handle. Traits supplies handle_type, invalid() and close().
template <class Traits>
class unique_handle {
public:
    using traits_type = Traits;
    using handle_type = typename Traits::handle_type;

    explicit unique_handle(handle_type h = Traits::invalid()) noexcept : h_(h) {}

    unique_handle(const unique_handle&) = delete;
    unique_handle& operator=(const unique_handle&) = delete;

    unique_handle(unique_handle&& other) noexcept : h_(std::exchange(other.h_, Traits::invalid())) {}

    unique_handle& operator=(unique_handle&& other) noexcept {
        if (this != &other) reset(other.release());
        return *this;
    }

    ~unique_handle() { reset(); }

    handle_type get() const noexcept { return h_; }
    explicit operator bool() const noexcept { return h_ != Traits::invalid(); }

    handle_type release() noexcept { return std::exchange(h_, Traits::invalid()); }

    // Same handle: no-op. Otherwise adopt h first, then close the old one.
    void reset(handle_type h = Traits::invalid()) noexcept {
        if (h_ == h) return;
        const handle_type old = std::exchange(h_, h);
        if (old != Traits::invalid()) Traits::close(old);
    }

    void swap(unique_handle& other) noexcept { std::swap(h_, other.h_); }

private:
    handle_type h_;
};

template <class Traits>
void swap(unique_handle<Traits>& a, unique_handle<Traits>& b) noexcept {
    a.swap(b);
}

struct FileTraits {
    using handle_type = std::FILE*;
    static handle_type invalid() noexcept { return nullptr; }
    static void close(handle_type f) noexcept { std::fclose(f); }
};

using FileHandle = unique_handle<FileTraits>;

// Opens path with fopen semantics. On failure returns errno as a std::error_code (generic_category).
std::expected<FileHandle, std::error_code> open_file(const std::filesystem::path& path, const char* mode);

}  // namespace cppref::raii_and_memory
