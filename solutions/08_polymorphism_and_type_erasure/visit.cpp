#include "visit.hpp"

#include <format>
#include <numbers>
#include <optional>
#include <string>
#include <variant>

namespace cppref::polymorphism {
namespace {

double circle_area(double radius) {
    return std::numbers::pi * radius * radius;
}

}  // namespace

Circle::Circle(double radius) : radius_(radius) {}

double Circle::radius() const {
    return radius_;
}

void Circle::accept(ShapeVisitor& visitor) const {
    // *this has static type const Circle&, so this selects visit(const Circle&).
    visitor.visit(*this);
}

Rect::Rect(double width, double height) : width_(width), height_(height) {}

double Rect::width() const {
    return width_;
}

double Rect::height() const {
    return height_;
}

void Rect::accept(ShapeVisitor& visitor) const {
    visitor.visit(*this);
}

void AreaVisitor::visit(const Circle& circle) {
    total_ += circle_area(circle.radius());
}

void AreaVisitor::visit(const Rect& rect) {
    total_ += rect.width() * rect.height();
}

double AreaVisitor::total() const {
    return total_;
}

double area_by_double_dispatch(const Shape& shape) {
    AreaVisitor visitor;
    shape.accept(visitor);
    return visitor.total();
}

double area(const ShapeValue& shape) {
    return std::visit(Overloaded{[](const Circle& c) { return circle_area(c.radius()); },
                                 [](const Rect& r) { return r.width() * r.height(); }},
                      shape);
}

std::string describe(const ShapeValue& shape) {
    return std::visit(Overloaded{[](const Circle& c) { return std::format("circle(radius={})", c.radius()); },
                                 [](const Rect& r) { return std::format("rect({}x{})", r.width(), r.height()); }},
                      shape);
}

std::optional<double> circle_radius(const Shape& shape) {
    if (const auto* circle = dynamic_cast<const Circle*>(&shape)) {
        return circle->radius();
    }
    return std::nullopt;
}

}  // namespace cppref::polymorphism
