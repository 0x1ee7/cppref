#include "config.hpp"
#include "errors.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace eh = cppref::error_handling;
namespace fs = std::filesystem;

namespace {

// Per-test scratch directory under the system temp dir, removed on destruction.
class ScratchDir {
public:
    ScratchDir()
        : path_(fs::temp_directory_path() /
                ("cppref-ex10-" + std::string(::testing::UnitTest::GetInstance()->current_test_info()->name()))) {
        fs::remove_all(path_);
        fs::create_directories(path_);
    }
    ~ScratchDir() {
        std::error_code ec;
        fs::remove_all(path_, ec);
    }
    ScratchDir(const ScratchDir&) = delete;
    ScratchDir& operator=(const ScratchDir&) = delete;

    [[nodiscard]] fs::path write(const std::string& name, const std::string& text) const {
        const fs::path file = path_ / name;
        std::ofstream out(file, std::ios::binary);
        out << text;
        return file;
    }
    [[nodiscard]] const fs::path& path() const { return path_; }

private:
    fs::path path_;
};

// True when `outer` carries a nested exception of type Inner.
template <typename Inner>
bool nested_is(const std::exception& outer) {
    const auto* ne = dynamic_cast<const std::nested_exception*>(&outer);
    if (ne == nullptr || ne->nested_ptr() == nullptr) {
        return false;
    }
    try {
        std::rethrow_exception(ne->nested_ptr());
    } catch (const Inner&) {
        return true;
    } catch (...) {
        return false;
    }
}

bool contains(const std::string& text, const std::string& needle) {
    return text.find(needle) != std::string::npos;
}

}  // namespace

// --- Part 1: parsing and typed lookup ----------------------------------------

TEST(Part1Hierarchy, ParseFlattensSectionsIntoDottedKeys) {
    const auto cfg = eh::parse_config("name = svc\n[db]\nhost = localhost\n[db.pool]\nsize = 8\n");
    ASSERT_EQ(cfg.size(), 3u);
    EXPECT_EQ(cfg.at("name"), "svc");
    EXPECT_EQ(cfg.at("db.host"), "localhost");
    EXPECT_EQ(cfg.at("db.pool.size"), "8");
}

TEST(Part1Hierarchy, CommentsAndBlankLinesAreIgnored) {
    const auto cfg = eh::parse_config("# header\n\n[app]\n# note\nname = x\n\n");
    ASSERT_EQ(cfg.size(), 1u);
    EXPECT_EQ(cfg.at("app.name"), "x");
}

TEST(Part1Hierarchy, BadLineReportsItsOneBasedNumber) {
    try {
        (void)eh::parse_config("[a]\nk = v\nnot a pair\n");
        FAIL() << "expected ParseError";
    } catch (const eh::ParseError& e) {
        EXPECT_EQ(e.line(), 3u);
    }
}

TEST(Part1Hierarchy, DuplicateKeyIsAParseError) {
    EXPECT_THROW((void)eh::parse_config("[a]\nk = 1\nk = 2\n"), eh::ParseError);
}

TEST(Part1Hierarchy, GetSettingConvertsIntegersAndReturnsStrings) {
    const eh::Config cfg{{"db.pool.size", "8"}, {"db.host", "localhost"}};
    EXPECT_EQ(eh::get_setting<int>(cfg, "db.pool.size"), 8);
    EXPECT_EQ(eh::get_setting<std::string>(cfg, "db.host"), "localhost");
}

TEST(Part1Hierarchy, GetSettingMissingKeyCarriesTheKey) {
    const eh::Config cfg{};
    try {
        (void)eh::get_setting<std::string>(cfg, "db.host");
        FAIL() << "expected SettingMissing";
    } catch (const eh::SettingMissing& e) {
        EXPECT_EQ(e.key(), "db.host");
        EXPECT_TRUE(contains(e.what(), "db.host"));
    }
}

TEST(Part1Hierarchy, GetSettingBadIntegerIsConfigInvalidNamingTheKey) {
    const eh::Config cfg{{"db.pool.size", "eight"}};
    try {
        (void)eh::get_setting<int>(cfg, "db.pool.size");
        FAIL() << "expected ConfigInvalid";
    } catch (const eh::ConfigInvalid& e) {
        EXPECT_TRUE(contains(e.what(), "db.pool.size"));
    }
}

// --- Part 2: exception chaining across the file boundary ---------------------

TEST(Part2Chaining, LoadsValidFile) {
    ScratchDir dir;
    const auto file = dir.write("app.ini", "[db]\nhost = localhost\n");
    const auto cfg = eh::load_config(file);
    EXPECT_EQ(cfg.at("db.host"), "localhost");
}

TEST(Part2Chaining, MissingFileIsConfigNotFoundWithPath) {
    ScratchDir dir;
    const fs::path file = dir.path() / "missing.ini";
    try {
        (void)eh::load_config(file);
        FAIL() << "expected ConfigNotFound";
    } catch (const eh::ConfigNotFound& e) {
        EXPECT_TRUE(contains(e.what(), "missing.ini"));
    }
}

TEST(Part2Chaining, MissingFileNestsTheOsError) {
    ScratchDir dir;
    try {
        (void)eh::load_config(dir.path() / "missing.ini");
        FAIL() << "expected ConfigNotFound";
    } catch (const eh::ConfigNotFound& e) {
        EXPECT_TRUE(nested_is<std::system_error>(e));
    }
}

TEST(Part2Chaining, DirectoryIsConfigUnreadable) {
    ScratchDir dir;
    try {
        (void)eh::load_config(dir.path());
        FAIL() << "expected ConfigUnreadable";
    } catch (const eh::ConfigUnreadable& e) {
        EXPECT_TRUE(contains(e.what(), "cannot read config"));
    }
}

TEST(Part2Chaining, SyntaxErrorIsConfigInvalidNestingParseError) {
    ScratchDir dir;
    const auto file = dir.write("bad.ini", "[a]\nbroken line\n");
    try {
        (void)eh::load_config(file);
        FAIL() << "expected ConfigInvalid";
    } catch (const eh::ConfigInvalid& e) {
        EXPECT_TRUE(contains(e.what(), "bad.ini"));
        EXPECT_TRUE(nested_is<eh::ParseError>(e));
    }
}

TEST(Part2Chaining, DescribeChainOfPlainExceptionIsItsMessage) {
    const std::runtime_error e("boom");
    EXPECT_EQ(eh::describe_chain(e), std::vector<std::string>{"boom"});
}

TEST(Part2Chaining, DescribeChainWalksLoadConfigNesting) {
    ScratchDir dir;
    try {
        (void)eh::load_config(dir.path() / "missing.ini");
        FAIL() << "expected ConfigNotFound";
    } catch (const eh::ConfigNotFound& e) {
        const auto chain = eh::describe_chain(e);
        ASSERT_GE(chain.size(), 2u);
        EXPECT_TRUE(contains(chain.front(), "config file not found"));
    }
}

TEST(Part2Chaining, DescribeChainEndsAtNonStdException) {
    struct Opaque {};
    try {
        try {
            throw Opaque{};
        } catch (...) {
            std::throw_with_nested(std::runtime_error("outer"));
        }
    } catch (const std::runtime_error& e) {
        const auto chain = eh::describe_chain(e);
        ASSERT_EQ(chain.size(), 2u);
        EXPECT_EQ(chain[0], "outer");
        EXPECT_EQ(chain[1], "unknown exception");
    }
}
