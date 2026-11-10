// Part 6: visitors - std::variant + std::visit versus classic double dispatch,
// and dynamic_cast on a polymorphic base.
#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <variant>

#include "visit.hpp"

namespace poly = cppref::polymorphism;

namespace {

constexpr double kPi = 3.14159265358979323846;

class Namer final : public poly::ShapeVisitor {
public:
    void visit(const poly::Circle& /*circle*/) override { name_ = "circle"; }
    void visit(const poly::Rect& /*rect*/) override { name_ = "rect"; }

    [[nodiscard]] const std::string& name() const { return name_; }

private:
    std::string name_;
};

}  // namespace

TEST(Part6Dispatch, AreaByDoubleDispatchForCircle) {
    const poly::Circle c(2.0);
    EXPECT_NEAR(poly::area_by_double_dispatch(c), kPi * 4.0, 1e-9);
}

TEST(Part6Dispatch, AreaByDoubleDispatchForRect) {
    const poly::Rect r(2.0, 3.0);
    EXPECT_DOUBLE_EQ(poly::area_by_double_dispatch(r), 6.0);
}

TEST(Part6Dispatch, AreaVisitorSumsEveryVisitedShape) {
    poly::AreaVisitor v;
    const poly::Rect r(2.0, 3.0);
    const poly::Circle c(1.0);
    r.accept(v);
    c.accept(v);
    EXPECT_NEAR(v.total(), 6.0 + kPi, 1e-9);
}

TEST(Part6Dispatch, VariantAreaMatchesTheHierarchy) {
    const poly::ShapeValue rect = poly::Rect(2.0, 3.0);
    EXPECT_DOUBLE_EQ(poly::area(rect), 6.0);
    const poly::ShapeValue circle = poly::Circle(1.0);
    EXPECT_NEAR(poly::area(circle), kPi, 1e-9);
}

TEST(Part6Dispatch, DescribeFormatsEachAlternative) {
    EXPECT_EQ(poly::describe(poly::ShapeValue{poly::Circle{2.5}}), "circle(radius=2.5)");
    EXPECT_EQ(poly::describe(poly::ShapeValue{poly::Rect{2.0, 3.0}}), "rect(2x3)");
}

TEST(Part6Dispatch, OverloadedVisitMatchesArea) {
    const poly::ShapeValue v = poly::Rect(2.0, 3.0);
    const double via_overloaded = std::visit(
        poly::Overloaded{[](const poly::Circle& c) { return kPi * c.radius() * c.radius(); },
                         [](const poly::Rect& r) { return r.width() * r.height(); }},
        v);
    EXPECT_DOUBLE_EQ(poly::area(v), via_overloaded);
}

TEST(Part6Dispatch, CircleRadiusUsesDynamicCastOnBaseReference) {
    const poly::Circle c(2.5);
    const poly::Shape& as_shape = c;
    const std::optional<double> r = poly::circle_radius(as_shape);
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(*r, 2.5);

    const poly::Rect rect(1.0, 1.0);
    EXPECT_FALSE(poly::circle_radius(rect).has_value());
}

TEST(Part6Dispatch, AcceptPassesTheStaticTypeToTheVisitor) {
    Namer namer;
    const poly::Rect rect(1.0, 1.0);
    const poly::Shape& as_shape = rect;
    as_shape.accept(namer);
    EXPECT_EQ(namer.name(), "rect");

    const poly::Circle circle(1.0);
    circle.accept(namer);
    EXPECT_EQ(namer.name(), "circle");
}
