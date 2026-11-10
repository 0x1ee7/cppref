#include "hiding.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <string_view>

using namespace cppref::hiding;

// --- Part4b: name hiding and using-declarations ---

TEST(Part4Hiding, BaseFormatsInt) {
    Formatter f;
    EXPECT_EQ(f.format(42), "#42");
}

TEST(Part4Hiding, DerivedFormatsStringView) {
    QuotedFormatter q;
    EXPECT_EQ(q.format(std::string_view("hi")), "\"hi\"");
    EXPECT_EQ(q.format(std::string("x")), "\"x\"");
}

TEST(Part4Hiding, BaseOverloadStaysReachableThroughBaseReference) {
    QuotedFormatter q;
    const Formatter& base = q;
    EXPECT_EQ(base.format(42), "#42");
}

TEST(Part4Hiding, UsingDeclarationRestoresIntOverloadOnDerived) {
    // Passes only once QuotedFormatter brings Formatter::format into scope with a
    // using-declaration, so that q.format(42) compiles.
    EXPECT_TRUE(CanFormatInt<QuotedFormatter>);
    EXPECT_TRUE(CanFormatInt<Formatter>);
}

TEST(Part4Hiding, DerivedThroughBasePointerDestroysCleanly) {
    std::unique_ptr<Formatter> p = std::make_unique<QuotedFormatter>();
    EXPECT_EQ(p->format(7), "#7");
}
