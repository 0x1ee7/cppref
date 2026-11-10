#include "plugins.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>

using namespace cppref::plugins;

namespace {

struct EchoPlugin : Plugin {
    std::string run(std::string_view input) override { return std::string(input); }
};

// Namespace-scope registration: runs during static initialisation, before main().
const Registrar kEchoRegistrar{"static_echo", [] { return std::make_unique<EchoPlugin>(); }};

}  // namespace

// This file is a separate translation unit from plugins_test.cpp. Its registration must
// already be visible when tests run.
TEST(Part6Registration, StaticRegistrarRegistersAtStartup) {
    auto p = PluginRegistry::global().create("static_echo");
    ASSERT_TRUE(p != nullptr);
    EXPECT_EQ(p->run("hi"), "hi");
}
