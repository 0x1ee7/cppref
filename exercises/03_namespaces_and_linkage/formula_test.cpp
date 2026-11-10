#include "formula.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

using namespace cppref::formula;

namespace {

Environment make_env() {
    Environment env = standard_environment();
    env.set_cell("A1", 2);
    env.set_cell("B1", 3);
    env.set_cell("C1", 0);
    return env;
}

ErrorKind error_kind(const std::expected<double, Error>& r) {
    return r.error().kind;
}

}  // namespace

// --- Part7: tokenizer, recursive-descent AST, explicit environments, std::expected ---

TEST(Part7Formula, LiteralsAndArithmeticPrecedence) {
    const Environment env = make_env();
    EXPECT_EQ(evaluate("42", env), 42.0);
    EXPECT_EQ(evaluate("1.5", env), 1.5);
    EXPECT_EQ(evaluate("1+2*3", env), 7.0);
    EXPECT_EQ(evaluate("(1+2)*3", env), 9.0);
    EXPECT_EQ(evaluate("-2*-3", env), 6.0);
    EXPECT_EQ(evaluate("8/2/2", env), 2.0);
    EXPECT_EQ(evaluate("2 - 3 - 4", env), -5.0);
}

TEST(Part7Formula, CellReferencesResolveFromEnvironment) {
    const Environment env = make_env();
    EXPECT_EQ(evaluate("A1*10", env), 20.0);
    EXPECT_EQ(env.cell("B1"), 3.0);
    EXPECT_FALSE(env.cell("nope").has_value());
}

TEST(Part7Formula, UnknownCellIsError) {
    EXPECT_EQ(error_kind(evaluate("Z9", make_env())), ErrorKind::unknown_cell);
}

TEST(Part7Formula, CellsAndFunctionsAreSeparateNamespaces) {
    Environment env = make_env();
    env.set_cell("SUM", 5);
    EXPECT_EQ(evaluate("SUM", env), 5.0);            // cell
    EXPECT_EQ(evaluate("SUM(A1,B1)", env), 5.0);     // function, not the cell
    EXPECT_EQ(error_kind(evaluate("A1(1)", env)), ErrorKind::unknown_function);
}

TEST(Part7Formula, VariadicSum) {
    const Environment env = make_env();
    EXPECT_EQ(evaluate("SUM()", env), 0.0);
    EXPECT_EQ(evaluate("SUM(1,2,3)", env), 6.0);
    EXPECT_EQ(evaluate("SUM(A1,B1,4)", env), 9.0);
}

TEST(Part7Formula, BuiltinFunctionValuesAndArity) {
    const Environment env = make_env();
    EXPECT_EQ(evaluate("ABS(-5)", env), 5.0);
    EXPECT_EQ(evaluate("MIN(3,7)", env), 3.0);
    EXPECT_EQ(evaluate("MAX(3,7)", env), 7.0);
    EXPECT_EQ(error_kind(evaluate("ABS(1,2)", env)), ErrorKind::arity);
    EXPECT_EQ(error_kind(evaluate("MIN(1)", env)), ErrorKind::arity);
}

TEST(Part7Formula, UnknownFunctionIsError) {
    EXPECT_EQ(error_kind(evaluate("FOO(1)", make_env())), ErrorKind::unknown_function);
}

TEST(Part7Formula, DivisionByZeroIsError) {
    const Environment env = make_env();
    EXPECT_EQ(error_kind(evaluate("1/0", env)), ErrorKind::division_by_zero);
    EXPECT_EQ(error_kind(evaluate("1/C1", env)), ErrorKind::division_by_zero);
}

TEST(Part7Formula, MalformedInputIsSyntaxError) {
    const Environment env = make_env();
    for (std::string_view bad : {"", "1+", "(1", "1 2", "SUM(1,", "1..2"}) {
        EXPECT_EQ(error_kind(evaluate(bad, env)), ErrorKind::syntax) << "input: " << bad;
    }
}

TEST(Part7Formula, ReferencedCellsAreDistinctAndSorted) {
    auto r = referenced_cells("A1+SUM(B1,A1)");
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(*r, (std::vector<std::string>{"A1", "B1"}));
}

TEST(Part7Formula, ReferencedCellsEmptyForConstantsAndReportsSyntax) {
    auto constant = referenced_cells("1+2");
    ASSERT_TRUE(constant.has_value());
    EXPECT_TRUE(constant->empty());

    auto bad = referenced_cells("1+");
    ASSERT_FALSE(bad.has_value());
    EXPECT_EQ(bad.error().kind, ErrorKind::syntax);
}

TEST(Part7Formula, SetCellOverwritesPreviousValue) {
    Environment env;
    env.set_cell("A1", 1);
    env.set_cell("A1", 9);
    EXPECT_EQ(env.cell("A1"), 9.0);
    EXPECT_EQ(evaluate("A1", env), 9.0);
}
