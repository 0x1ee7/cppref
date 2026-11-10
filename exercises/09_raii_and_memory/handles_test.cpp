// Part 1-3: C handles, rule of five, unique_ptr with a stateful deleter.
#include "buffer.hpp"
#include "unique_handle.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace rr = cppref::raii_and_memory;
namespace {
namespace fs = std::filesystem;

// Instrumented policy: every close() is recorded, so tests can prove exactly-once release.
struct RecordingTraits {
    using handle_type = int;
    static int invalid() noexcept { return -1; }
    static void close(int h) noexcept { closed().push_back(h); }
    static std::vector<int>& closed() {
        static std::vector<int> v;
        return v;
    }
};
using RecHandle = rr::unique_handle<RecordingTraits>;

fs::path scratch_file(const char* stem) {
    const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
    return fs::temp_directory_path() / (std::string("cppref09_") + stem + "_" + info->name() + ".bin");
}

// ---------------------------------------------------------------------------
// Part 1 - unique_handle<Traits> and FileHandle
// ---------------------------------------------------------------------------
class Part1Handles : public ::testing::Test {
protected:
    void SetUp() override { RecordingTraits::closed().clear(); }
};

TEST_F(Part1Handles, DefaultOwnsNothing) {
    RecHandle h;
    EXPECT_FALSE(h);
    EXPECT_EQ(h.get(), -1);
    EXPECT_TRUE(RecordingTraits::closed().empty());
    h.reset(3);
    EXPECT_EQ(h.get(), 3);
    EXPECT_EQ(h.release(), 3);
    EXPECT_TRUE(RecordingTraits::closed().empty());
}

TEST_F(Part1Handles, AdoptedHandleClosedOnScopeExit) {
    {
        RecHandle h{7};
        EXPECT_TRUE(h);
        EXPECT_EQ(h.get(), 7);
        EXPECT_TRUE(RecordingTraits::closed().empty());
    }
    EXPECT_EQ(RecordingTraits::closed(), (std::vector<int>{7}));
}

TEST_F(Part1Handles, MoveConstructorTransfersOwnership) {
    {
        RecHandle a{3};
        RecHandle b{std::move(a)};
        EXPECT_FALSE(a);
        EXPECT_EQ(b.get(), 3);
    }
    EXPECT_EQ(RecordingTraits::closed(), (std::vector<int>{3}));
}

TEST_F(Part1Handles, MoveAssignClosesDestinationOnce) {
    {
        RecHandle a{1};
        RecHandle b{2};
        b = std::move(a);
        EXPECT_EQ(RecordingTraits::closed(), (std::vector<int>{2}));
        EXPECT_EQ(b.get(), 1);
        EXPECT_FALSE(a);
    }
    EXPECT_EQ(RecordingTraits::closed(), (std::vector<int>{2, 1}));
}

TEST_F(Part1Handles, SelfMoveAssignKeepsHandle) {
    {
        RecHandle a{5};
        RecHandle& alias = a;
        a = std::move(alias);
        EXPECT_EQ(a.get(), 5);
        EXPECT_TRUE(RecordingTraits::closed().empty());
    }
    EXPECT_EQ(RecordingTraits::closed(), (std::vector<int>{5}));
}

TEST_F(Part1Handles, ReleaseGivesUpOwnershipWithoutClose) {
    RecHandle a{9};
    EXPECT_EQ(a.release(), 9);
    EXPECT_FALSE(a);
    EXPECT_TRUE(RecordingTraits::closed().empty());
}

TEST_F(Part1Handles, ResetClosesOldAndAdoptsNew) {
    RecHandle a{4};
    a.reset(6);
    EXPECT_EQ(RecordingTraits::closed(), (std::vector<int>{4}));
    EXPECT_EQ(a.get(), 6);
}

TEST_F(Part1Handles, ResetToCurrentHandleDoesNotClose) {
    // Closing first and then adopting the same value would leave a dangling owner.
    RecHandle a{4};
    a.reset(a.get());
    EXPECT_TRUE(RecordingTraits::closed().empty());
    EXPECT_EQ(a.get(), 4);
}

TEST_F(Part1Handles, ResetWithoutArgumentEmpties) {
    RecHandle a{8};
    a.reset();
    EXPECT_FALSE(a);
    EXPECT_EQ(RecordingTraits::closed(), (std::vector<int>{8}));
}

TEST_F(Part1Handles, SwapExchangesOwnershipWithoutClose) {
    {
        RecHandle a{1};
        RecHandle b{2};
        swap(a, b);
        EXPECT_EQ(a.get(), 2);
        EXPECT_EQ(b.get(), 1);
        EXPECT_TRUE(RecordingTraits::closed().empty());
    }
    EXPECT_EQ(RecordingTraits::closed().size(), 2u);
}

TEST_F(Part1Handles, OpenFileReadsThenClosesOnScopeExit) {
    const fs::path p = scratch_file("read");
    { std::ofstream(p, std::ios::binary) << "A"; }
    {
        auto f = rr::open_file(p, "rb");
        ASSERT_TRUE(f.has_value());
        EXPECT_EQ(std::fgetc(f->get()), 'A');
    }
    // Windows refuses to delete an open file, so removal proves the FILE* was closed.
    EXPECT_TRUE(fs::remove(p));
}

TEST_F(Part1Handles, OpenFileMissingReturnsErrno) {
    const fs::path p = scratch_file("missing");
    fs::remove(p);
    auto f = rr::open_file(p, "rb");
    ASSERT_FALSE(f.has_value());
    EXPECT_EQ(f.error(), std::make_error_code(std::errc::no_such_file_or_directory));
}

TEST_F(Part1Handles, FileHandleIsMoveOnlyAndNothrowMovable) {
    static_assert(!std::is_copy_constructible_v<rr::FileHandle>);
    static_assert(!std::is_copy_assignable_v<rr::FileHandle>);
    static_assert(std::is_nothrow_move_constructible_v<rr::FileHandle>);
    static_assert(std::is_nothrow_move_assignable_v<rr::FileHandle>);
    const fs::path path = fs::temp_directory_path() / "cppref_09_move_only.bin";
    auto opened = rr::open_file(path, "wb");
    ASSERT_TRUE(opened.has_value());
    rr::FileHandle moved(std::move(*opened));
    EXPECT_TRUE(moved);
    moved.reset();
    fs::remove(path);
}

// ---------------------------------------------------------------------------
// Part 2 - rule of five on ByteBuffer
// ---------------------------------------------------------------------------
class Part2RuleOfFive : public ::testing::Test {};

TEST_F(Part2RuleOfFive, DefaultIsEmpty) {
    rr::ByteBuffer b;
    EXPECT_EQ(b.size(), 0u);
    EXPECT_EQ(b.capacity(), 0u);
    rr::ByteBuffer copy(b);
    EXPECT_EQ(copy.size(), 0u);
}

TEST_F(Part2RuleOfFive, SizedConstructorZeroFills) {
    rr::ByteBuffer b(16);
    ASSERT_EQ(b.size(), 16u);
    for (std::byte x : b.bytes()) EXPECT_EQ(x, std::byte{0});
}

TEST_F(Part2RuleOfFive, SpanConstructorCopiesSource) {
    std::vector<std::byte> src{std::byte{1}, std::byte{2}, std::byte{3}};
    rr::ByteBuffer b{std::span<const std::byte>(src)};
    src[0] = std::byte{99};
    ASSERT_EQ(b.size(), 3u);
    EXPECT_EQ(b.bytes()[0], std::byte{1});
}

TEST_F(Part2RuleOfFive, CopyIsDeep) {
    std::vector<std::byte> src{std::byte{1}, std::byte{2}};
    rr::ByteBuffer a{std::span<const std::byte>(src)};
    rr::ByteBuffer b(a);
    EXPECT_NE(a.data(), b.data());
    EXPECT_EQ(b.size(), 2u);
    b.data()[0] = std::byte{0xFF};
    EXPECT_EQ(a.bytes()[0], std::byte{1});
}

TEST_F(Part2RuleOfFive, CopyOfEmptyIsEmpty) {
    rr::ByteBuffer a;
    rr::ByteBuffer b(a);
    EXPECT_EQ(b.size(), 0u);
}

TEST_F(Part2RuleOfFive, MoveStealsStorageAndEmptiesSource) {
    rr::ByteBuffer a(8);
    const std::byte* old = a.data();
    rr::ByteBuffer b(std::move(a));
    EXPECT_EQ(a.size(), 0u);
    EXPECT_EQ(a.capacity(), 0u);
    EXPECT_EQ(b.size(), 8u);
    EXPECT_EQ(b.data(), old);
}

TEST_F(Part2RuleOfFive, MoveAssignReleasesOldAndTakesSource) {
    rr::ByteBuffer a(4);
    rr::ByteBuffer b(9);
    const std::byte* old = a.data();
    b = std::move(a);
    EXPECT_EQ(b.size(), 4u);
    EXPECT_EQ(b.data(), old);
    EXPECT_EQ(a.size(), 0u);
}

TEST_F(Part2RuleOfFive, SelfMoveAssignPreservesContents) {
    rr::ByteBuffer a(5);
    rr::ByteBuffer& alias = a;
    a = std::move(alias);
    EXPECT_EQ(a.size(), 5u);
}

TEST_F(Part2RuleOfFive, SelfCopyAssignPreservesContents) {
    std::vector<std::byte> src{std::byte{7}};
    rr::ByteBuffer a{std::span<const std::byte>(src)};
    rr::ByteBuffer& alias = a;
    a = alias;
    ASSERT_EQ(a.size(), 1u);
    EXPECT_EQ(a.bytes()[0], std::byte{7});
}

TEST_F(Part2RuleOfFive, CopyAssignReplacesContents) {
    std::vector<std::byte> src{std::byte{5}, std::byte{6}};
    rr::ByteBuffer a{std::span<const std::byte>(src)};
    rr::ByteBuffer b(10);
    b = a;
    EXPECT_EQ(b.size(), 2u);
    EXPECT_EQ(b.bytes()[1], std::byte{6});
}

TEST_F(Part2RuleOfFive, MoveOperationsAreNoexcept) {
    static_assert(std::is_nothrow_move_constructible_v<rr::ByteBuffer>);
    static_assert(std::is_nothrow_move_assignable_v<rr::ByteBuffer>);
    rr::ByteBuffer src(std::size_t{4});
    rr::ByteBuffer dst(std::move(src));
    EXPECT_EQ(dst.size(), 4u);
    EXPECT_EQ(src.size(), 0u);
}

TEST_F(Part2RuleOfFive, AppendKeepsEarlierBytesAcrossGrowth) {
    rr::ByteBuffer b;
    std::size_t total = 0;
    for (int i = 0; i < 1000; ++i) {
        std::vector<std::byte> chunk(7, static_cast<std::byte>(i & 0xFF));
        b.append(chunk);
        total += chunk.size();
    }
    ASSERT_EQ(b.size(), total);
    EXPECT_GE(b.capacity(), b.size());
    EXPECT_EQ(b.bytes()[0], std::byte{0});
    EXPECT_EQ(b.bytes()[7], std::byte{1});
    EXPECT_EQ(b.bytes()[total - 1], static_cast<std::byte>(999 & 0xFF));
}

TEST_F(Part2RuleOfFive, AppendEmptySpanIsNoop) {
    rr::ByteBuffer b(3);
    b.append({});
    EXPECT_EQ(b.size(), 3u);
}

// ---------------------------------------------------------------------------
// Part 3 - unique_ptr with a stateful deleter and array ownership
// ---------------------------------------------------------------------------
class Part3UniquePtr : public ::testing::Test {};

TEST_F(Part3UniquePtr, DeleterRunsOnceAtScopeExit) {
    std::size_t freed = 0;
    {
        auto p = rr::make_scratch(4, &freed);
        ASSERT_NE(p, nullptr);
        p[0] = 1;
        p[3] = 4;
        EXPECT_EQ(freed, 0u);
    }
    EXPECT_EQ(freed, 1u);
}

TEST_F(Part3UniquePtr, ZeroLengthArrayIsStillOwned) {
    std::size_t freed = 0;
    { auto p = rr::make_scratch(0, &freed); }
    EXPECT_EQ(freed, 1u);
}

TEST_F(Part3UniquePtr, MoveTransfersWithoutDoubleFree) {
    std::size_t freed = 0;
    {
        auto a = rr::make_scratch(2, &freed);
        auto b = std::move(a);
        EXPECT_EQ(a, nullptr);
        EXPECT_NE(b, nullptr);
    }
    EXPECT_EQ(freed, 1u);
}

TEST_F(Part3UniquePtr, ReleaseSkipsDeleter) {
    std::size_t freed = 0;
    auto p = rr::make_scratch(3, &freed);
    int* raw = p.release();
    EXPECT_EQ(freed, 0u);
    delete[] raw;  // the caller now owns the raw allocation
    EXPECT_EQ(freed, 0u);
}

TEST_F(Part3UniquePtr, ResetInvokesDeleter) {
    std::size_t freed = 0;
    auto p = rr::make_scratch(3, &freed);
    p.reset();
    EXPECT_EQ(freed, 1u);
    EXPECT_EQ(p, nullptr);
}

TEST_F(Part3UniquePtr, ScratchPtrIsMoveOnly) {
    static_assert(!std::is_copy_constructible_v<rr::ScratchPtr>);
    static_assert(std::is_move_constructible_v<rr::ScratchPtr>);
    std::size_t freed = 0;
    auto a = rr::make_scratch(2, &freed);
    rr::ScratchPtr b(std::move(a));
    EXPECT_EQ(a, nullptr);
    EXPECT_NE(b, nullptr);
}

}  // namespace
