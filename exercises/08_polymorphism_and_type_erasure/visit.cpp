#include "visit.hpp"

#include <stdexcept>
#include <string>

namespace cppref::polymorphism {
namespace {
[[noreturn]] void not_implemented(const char* what) {
    throw std::logic_error(std::string("not implemented: ") + what);
}
}  // namespace

Circle::Circle(double /*radius*/) { not_implemented("Circle::Circle"); }

double Circle::radius() const { not_implemented("Circle::radius"); }

void Circle::accept(ShapeVisitor& /*visitor*/) const { not_implemented("Circle::accept"); }

Rect::Rect(double /*width*/, double /*height*/) { not_implemented("Rect::Rect"); }

double Rect::width() const { not_implemented("Rect::width"); }

double Rect::height() const { not_implemented("Rect::height"); }

void Rect::accept(ShapeVisitor& /*visitor*/) const { not_implemented("Rect::accept"); }

void AreaVisitor::visit(const Circle& /*circle*/) { not_implemented("AreaVisitor::visit(Circle)"); }

void AreaVisitor::visit(const Rect& /*rect*/) { not_implemented("AreaVisitor::visit(Rect)"); }

double AreaVisitor::total() const { not_implemented("AreaVisitor::total"); }

double area_by_double_dispatch(const Shape& /*shape*/) { not_implemented("area_by_double_dispatch"); }

double area(const ShapeValue& /*shape*/) { not_implemented("area"); }

std::string describe(const ShapeValue& /*shape*/) { not_implemented("describe"); }

std::optional<double> circle_radius(const Shape& /*shape*/) { not_implemented("circle_radius"); }

}  // namespace cppref::polymorphism
