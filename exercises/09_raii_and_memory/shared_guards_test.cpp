// Part 4-5: shared_ptr/weak_ptr document graph, scope guards, TempDir.
#include "document.hpp"
#include "guards.hpp"
#include "temp_dir.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace rr = cppref::raii_and_memory;
namespace {
namespace fs = std::filesystem;

constexpr std::string_view kDoc = "intro\n# A\na1\na2\n# B\nb1";

// ---------------------------------------------------------------------------
// Part 4 - shared/weak ownership: sections, aliasing ctor, weak observer cache
// ---------------------------------------------------------------------------
class Part4Shared : public ::testing::Test {};

TEST_F(Part4Shared, ParseSplitsPreambleAndSections) {
    auto doc = rr::Document::parse(kDoc);
    ASSERT_EQ(doc->sections().size(), 3u);
    EXPECT_EQ(doc->sections()[0]->heading, "");
    EXPECT_EQ(doc->sections()[0]->body, "intro");
    EXPECT_EQ(doc->sections()[1]->heading, "A");
    EXPECT_EQ(doc->sections()[1]->body, "a1\na2");
    EXPECT_EQ(doc->sections()[2]->body, "b1");
}

TEST_F(Part4Shared, BlankPreambleIsOmitted) {
    auto doc = rr::Document::parse("# A\nx");
    ASSERT_EQ(doc->sections().size(), 1u);
    EXPECT_EQ(doc->sections()[0]->heading, "A");
}

TEST_F(Part4Shared, EmptyTextHasNoSections) {
    auto doc = rr::Document::parse("");
    EXPECT_TRUE(doc->sections().empty());
}

TEST_F(Part4Shared, SectionBacklinksToLiveDocument) {
    auto doc = rr::Document::parse(kDoc);
    auto sec = doc->sections()[1];
    EXPECT_EQ(sec->document().get(), doc.get());
}

TEST_F(Part4Shared, SectionBacklinkNullAfterDocumentGone) {
    std::weak_ptr<const rr::Document> w;
    std::shared_ptr<const rr::Section> sec;
    {
        auto doc = rr::Document::parse("# A\nx");
        w = doc;
        sec = doc->sections()[0];
    }
    // Sections must not own their document; otherwise doc <-> section is a leaked cycle.
    EXPECT_TRUE(w.expired());
    EXPECT_EQ(sec->document(), nullptr);
}

TEST_F(Part4Shared, BodyOfAliasesSectionControlBlock) {
    auto doc = rr::Document::parse(kDoc);
    auto sec = doc->sections()[1];
    auto body = rr::body_of(sec);
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(*body, "a1\na2");
    EXPECT_EQ(body.get(), &sec->body);
    EXPECT_EQ(body.use_count(), sec.use_count());
}

TEST_F(Part4Shared, BodyOfKeepsSectionAliveAfterDocumentDies) {
    auto body = [] {
        auto doc = rr::Document::parse("# A\nstill here");
        return rr::body_of(doc->sections()[0]);
    }();
    ASSERT_NE(body, nullptr);
    EXPECT_EQ(*body, "still here");
}

TEST_F(Part4Shared, BodyOfNullIsNull) {
    EXPECT_EQ(rr::body_of(nullptr), nullptr);
}

TEST_F(Part4Shared, CacheReturnsSameDocumentWhileAlive) {
    int parses = 0;
    rr::DocumentCache cache([&parses](std::string_view t) {
        ++parses;
        return rr::Document::parse(t);
    });
    auto a = cache.get_or_parse("k", kDoc);
    auto b = cache.get_or_parse("k", kDoc);
    EXPECT_EQ(a.get(), b.get());
    EXPECT_EQ(parses, 1);
    EXPECT_EQ(cache.live_count(), 1u);
}

TEST_F(Part4Shared, CacheDoesNotKeepDocumentAlive) {
    rr::DocumentCache cache;
    std::weak_ptr<const rr::Document> w;
    {
        auto d = cache.get_or_parse("k", "# A\nx");
        w = d;
    }
    EXPECT_TRUE(w.expired());
    EXPECT_EQ(cache.live_count(), 0u);
}

TEST_F(Part4Shared, CacheReparsesAfterExpiry) {
    int parses = 0;
    rr::DocumentCache cache([&parses](std::string_view t) {
        ++parses;
        return rr::Document::parse(t);
    });
    std::weak_ptr<const rr::Document> first;
    {
        auto d = cache.get_or_parse("k", "# A\nx");
        first = d;
    }
    auto again = cache.get_or_parse("k", "# A\nx");
    EXPECT_EQ(parses, 2);
    EXPECT_TRUE(first.expired());
}

TEST_F(Part4Shared, DistinctKeysParseSeparately) {
    int parses = 0;
    rr::DocumentCache cache([&parses](std::string_view t) {
        ++parses;
        return rr::Document::parse(t);
    });
    auto a = cache.get_or_parse("a", "# A\nx");
    auto b = cache.get_or_parse("b", "# A\nx");
    EXPECT_NE(a.get(), b.get());
    EXPECT_EQ(parses, 2);
    EXPECT_EQ(cache.live_count(), 2u);
}

TEST_F(Part4Shared, PurgeExpiredRemovesOnlyDeadEntries) {
    rr::DocumentCache cache;
    auto held = cache.get_or_parse("held", "# A\nx");
    cache.get_or_parse("gone", "# B\ny");  // temporary: expired immediately
    EXPECT_EQ(cache.entry_count(), 2u);
    EXPECT_EQ(cache.purge_expired(), 1u);
    EXPECT_EQ(cache.entry_count(), 1u);
    EXPECT_EQ(cache.live_count(), 1u);
}

TEST_F(Part4Shared, DefaultCacheUsesDocumentParse) {
    rr::DocumentCache cache;
    auto d = cache.get_or_parse("x", "# H\nb");
    ASSERT_NE(d, nullptr);
    EXPECT_EQ(d->sections().size(), 1u);
}

// ---------------------------------------------------------------------------
// Part 5 - scope guards (uncaught_exceptions) and TempDir
// ---------------------------------------------------------------------------
class Part5Guards : public ::testing::Test {};

TEST_F(Part5Guards, ScopeExitRunsOnNormalExit) {
    int n = 0;
    {
        rr::scope_exit g([&] { ++n; });
        EXPECT_EQ(n, 0);
    }
    EXPECT_EQ(n, 1);
}

TEST_F(Part5Guards, ScopeExitRunsWhenUnwinding) {
    int n = 0;
    try {
        rr::scope_exit g([&] { ++n; });
        throw std::runtime_error("boom");
    } catch (const std::runtime_error&) {
    }
    EXPECT_EQ(n, 1);
}

TEST_F(Part5Guards, ScopeExitReleaseDisarms) {
    int n = 0;
    {
        rr::scope_exit g([&] { ++n; });
        g.release();
    }
    EXPECT_EQ(n, 0);
}

TEST_F(Part5Guards, ScopeExitMovedFromRunsOnce) {
    int n = 0;
    {
        rr::scope_exit a([&] { ++n; });
        rr::scope_exit b(std::move(a));
    }
    EXPECT_EQ(n, 1);
}

TEST_F(Part5Guards, ScopeFailRunsOnlyWhenUnwinding) {
    int n = 0;
    {
        rr::scope_fail g([&] { ++n; });
    }
    EXPECT_EQ(n, 0);
    try {
        rr::scope_fail g([&] { ++n; });
        throw std::runtime_error("boom");
    } catch (const std::runtime_error&) {
    }
    EXPECT_EQ(n, 1);
}

TEST_F(Part5Guards, ScopeFailIgnoresExceptionCaughtInsideScope) {
    // The exception is gone before the scope ends, so the guard must stay quiet.
    int n = 0;
    {
        rr::scope_fail g([&] { ++n; });
        try {
            throw 42;
        } catch (int) {
        }
    }
    EXPECT_EQ(n, 0);
}

TEST_F(Part5Guards, ScopeFailReleaseDisarms) {
    int n = 0;
    try {
        rr::scope_fail g([&] { ++n; });
        g.release();
        throw std::runtime_error("boom");
    } catch (const std::runtime_error&) {
    }
    EXPECT_EQ(n, 0);
}

TEST_F(Part5Guards, ScopeSuccessRunsOnlyOnNormalExit) {
    int n = 0;
    {
        rr::scope_success g([&] { ++n; });
    }
    EXPECT_EQ(n, 1);
    try {
        rr::scope_success g([&] { ++n; });
        throw std::runtime_error("boom");
    } catch (const std::runtime_error&) {
    }
    EXPECT_EQ(n, 1);
}

TEST_F(Part5Guards, ScopeSuccessRunsWhenInnerExceptionWasCaught) {
    int n = 0;
    {
        rr::scope_success g([&] { ++n; });
        try {
            throw 1;
        } catch (int) {
        }
    }
    EXPECT_EQ(n, 1);
}

TEST_F(Part5Guards, ScopeSuccessReleaseDisarms) {
    int n = 0;
    {
        rr::scope_success g([&] { ++n; });
        g.release();
    }
    EXPECT_EQ(n, 0);
}

class Part5TempDir : public ::testing::Test {};

TEST_F(Part5TempDir, CreatesDirectoryUnderTempRoot) {
    rr::TempDir d("cppref09");
    EXPECT_TRUE(fs::is_directory(d.path()));
    EXPECT_TRUE(fs::equivalent(d.path().parent_path(), fs::temp_directory_path()));
    EXPECT_EQ(d.path().filename().string().rfind("cppref09", 0), 0u);
}

TEST_F(Part5TempDir, TwoDirectoriesDiffer) {
    rr::TempDir a("cppref09");
    rr::TempDir b("cppref09");
    EXPECT_NE(a.path(), b.path());
}

TEST_F(Part5TempDir, FileJoinsNameUnderDirectory) {
    rr::TempDir d("cppref09");
    EXPECT_EQ(d.file("a.txt"), d.path() / "a.txt");
}

TEST_F(Part5TempDir, DestructorRemovesTreeRecursively) {
    fs::path saved;
    fs::path nested;
    {
        rr::TempDir d("cppref09");
        saved = d.path();
        nested = d.file("sub/x.txt");
        fs::create_directories(nested.parent_path());
        std::ofstream(nested) << "data";
        EXPECT_TRUE(fs::exists(nested));
    }
    EXPECT_FALSE(fs::exists(saved));
}

TEST_F(Part5TempDir, MoveConstructorTransfersOwnership) {
    fs::path p;
    {
        rr::TempDir a("cppref09");
        p = a.path();
        rr::TempDir b(std::move(a));
        EXPECT_TRUE(a.path().empty());
        EXPECT_EQ(b.path(), p);
        EXPECT_TRUE(fs::is_directory(p));
    }
    EXPECT_FALSE(fs::exists(p));
}

TEST_F(Part5TempDir, MoveAssignRemovesDestinationDirectory) {
    rr::TempDir a("cppref09");
    rr::TempDir b("cppref09");
    const fs::path pa = a.path();
    const fs::path pb = b.path();
    b = std::move(a);
    EXPECT_FALSE(fs::exists(pb));
    EXPECT_EQ(b.path(), pa);
    EXPECT_TRUE(a.path().empty());
}

TEST_F(Part5TempDir, SelfMoveAssignKeepsDirectory) {
    rr::TempDir a("cppref09");
    const fs::path p = a.path();
    rr::TempDir& alias = a;
    a = std::move(alias);
    EXPECT_EQ(a.path(), p);
    EXPECT_TRUE(fs::is_directory(p));
}

TEST_F(Part5TempDir, KeepDisownsDirectory) {
    fs::path p;
    {
        rr::TempDir d("cppref09");
        p = d.keep();
        EXPECT_TRUE(d.path().empty());
    }
    EXPECT_TRUE(fs::is_directory(p));
    fs::remove_all(p);
}

TEST_F(Part5TempDir, FileAfterKeepThrows) {
    rr::TempDir d("cppref09");
    fs::path p = d.keep();
    EXPECT_THROW(d.file("x"), std::logic_error);
    fs::remove_all(p);
}

}  // namespace
