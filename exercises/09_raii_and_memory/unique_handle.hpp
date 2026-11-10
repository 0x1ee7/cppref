#pragma once

#include <cstdio>
#include <expected>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <utility>

namespace cppref::raii_and_memory {

// Move-only owner of a C-style handle. The Traits policy supplies:
//   using handle_type = ...;                    // cheap, trivially copyable
//   static handle_type invalid() noexcept;      // sentinel: owns nothing
//   static void close(handle_type h) noexcept;  // release the resource; never called with invalid()
//
// Engineer-owned: keep the handle as the only private data member.
template <class Traits>
class unique_handle {
public:
    using traits_type = Traits;
    using handle_type = typename Traits::handle_type;

    // Adopts h. The default owns nothing. Does not throw.
    explicit unique_handle(handle_type h = Traits::invalid()) noexcept {
        (void)h;
    }

    unique_handle(const unique_handle&) = delete;
    unique_handle& operator=(const unique_handle&) = delete;

    // Takes ownership; other is left owning nothing. Does not throw.
    unique_handle(unique_handle&& other) noexcept { (void)other; }

    // Closes the current handle (if any), then takes other's. Self-move is a no-op. Does not throw.
    unique_handle& operator=(unique_handle&& other) noexcept {
        (void)other;
        return *this;
    }

    // Closes the handle if owned.
    ~unique_handle() = default;

    // The owned handle, or Traits::invalid().
    handle_type get() const noexcept { return Traits::invalid(); }

    // True when owning a valid handle.
    explicit operator bool() const noexcept { return false; }

    // Gives up ownership without closing. Afterwards owns nothing.
    handle_type release() noexcept { return Traits::invalid(); }

    // Closes the current handle, then adopts h. reset(get()) keeps ownership and closes nothing.
    void reset(handle_type h = Traits::invalid()) noexcept { (void)h; }

    void swap(unique_handle& other) noexcept { (void)other; }
};

template <class Traits>
void swap(unique_handle<Traits>& a, unique_handle<Traits>& b) noexcept {
    a.swap(b);
}

// C stdio FILE* policy. close() ignores fclose errors; use an explicit close if you need them.
struct FileTraits {
    using handle_type = std::FILE*;
    static handle_type invalid() noexcept { return nullptr; }
    static void close(handle_type f) noexcept { std::fclose(f); }
};

using FileHandle = unique_handle<FileTraits>;

// Opens path with fopen semantics. On failure returns errno as a std::error_code (generic_category).
std::expected<FileHandle, std::error_code> open_file(const std::filesystem::path& path, const char* mode);

}  // namespace cppref::raii_and_memory
