#include "plugins.hpp"
#include <cctype>

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

using namespace cppref::plugins;

namespace {

struct UpperPlugin : Plugin {
    std::string run(std::string_view input) override {
        std::string out(input);
        std::transform(out.begin(), out.end(), out.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return out;
    }
};

}  // namespace

// --- Part6b: process-wide registry (self-registration is tested in plugins_registration_test.cpp) ---

TEST(Part6Plugins, AddThenCreateByName) {
    PluginRegistry::global().add("test_upper_1", [] { return std::make_unique<UpperPlugin>(); });
    auto p = PluginRegistry::global().create("test_upper_1");
    ASSERT_TRUE(p != nullptr);
    EXPECT_EQ(p->run("abc"), "ABC");
}

TEST(Part6Plugins, CreateUnknownNameReturnsNull) {
    EXPECT_TRUE(PluginRegistry::global().create("test_missing_1") == nullptr);
}

TEST(Part6Plugins, DuplicateNameThrows) {
    auto& reg = PluginRegistry::global();
    reg.add("test_dup_1", [] { return std::make_unique<UpperPlugin>(); });
    EXPECT_THROW(reg.add("test_dup_1", [] { return std::make_unique<UpperPlugin>(); }),
                 std::invalid_argument);
}

TEST(Part6Plugins, NamesAreSorted) {
    auto& reg = PluginRegistry::global();
    reg.add("test_zz_2", [] { return std::make_unique<UpperPlugin>(); });
    reg.add("test_aa_2", [] { return std::make_unique<UpperPlugin>(); });
    const auto names = reg.names();
    EXPECT_TRUE(std::is_sorted(names.begin(), names.end()));
    EXPECT_NE(std::find(names.begin(), names.end(), "test_aa_2"), names.end());
    EXPECT_NE(std::find(names.begin(), names.end(), "test_zz_2"), names.end());
}

TEST(Part6Plugins, GlobalIsSingletonAndCreatesIndependentInstances) {
    auto& reg = PluginRegistry::global();
    EXPECT_EQ(&PluginRegistry::global(), &reg);
    reg.add("test_fresh_3", [] { return std::make_unique<UpperPlugin>(); });
    auto p1 = reg.create("test_fresh_3");
    auto p2 = reg.create("test_fresh_3");
    ASSERT_TRUE(p1 != nullptr && p2 != nullptr);
    EXPECT_NE(p1.get(), p2.get());
}
