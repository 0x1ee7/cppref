// Part 6 - visitors: std::variant + std::visit versus classic double dispatch,
// and the runtime cost of dynamic_cast on a polymorphic base.
#pragma once

#include <optional>
#include <string>
#include <variant>

namespace cppref::polymorphism {

class ShapeVisitor;

/// Polymorphic shape. accept() is the first half of double dispatch: it calls
/// back visitor.visit(*this) with the static type of the concrete class.
class Shape {
public:
    virtual ~Shape() = default;
    virtual void accept(ShapeVisitor& visitor) const = 0;
};

class Circle final : public Shape {
public:
    explicit Circle(double radius);
    [[nodiscard]] double radius() const;
    void accept(ShapeVisitor& visitor) const override;

private:
    double radius_;
};

class Rect final : public Shape {
public:
    Rect(double width, double height);
    [[nodiscard]] double width() const;
    [[nodiscard]] double height() const;
    void accept(ShapeVisitor& visitor) const override;

private:
    double width_;
    double height_;
};

/// Second half of double dispatch: one overload per concrete shape.
class ShapeVisitor {
public:
    virtual ~ShapeVisitor() = default;
    virtual void visit(const Circle& circle) = 0;
    virtual void visit(const Rect& rect) = 0;
};

/// Sums the areas of every shape it visits.
class AreaVisitor final : public ShapeVisitor {
public:
    void visit(const Circle& circle) override;
    void visit(const Rect& rect) override;

    [[nodiscard]] double total() const;

private:
    double total_ = 0.0;
};

/// Area of any shape, computed by double dispatch through AreaVisitor.
[[nodiscard]] double area_by_double_dispatch(const Shape& shape);

/// Overload set for std::visit. Lambdas are combined into one callable.
template <class... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overloaded(Ts...) -> Overloaded<Ts...>;

/// Value-semantic alternative to the class hierarchy.
using ShapeValue = std::variant<Circle, Rect>;

/// Area via std::visit with Overloaded.
[[nodiscard]] double area(const ShapeValue& shape);

/// Formats with std::format("{}", x):
///   circle -> "circle(radius=<r>)", rect -> "rect(<w>x<h>)".
[[nodiscard]] std::string describe(const ShapeValue& shape);

/// Radius of a Circle, found with dynamic_cast on the base reference. Empty otherwise.
[[nodiscard]] std::optional<double> circle_radius(const Shape& shape);

}  // namespace cppref::polymorphism
