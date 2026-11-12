#include "solution.hpp"

#include <format>
#include <memory>
#include <print>
#include <string>
#include <utility>
#include <vector>

// rust: enum Shape { Circle { r: f64 }, Rect { w: f64, h: f64 }, Dot }
struct ShapeSpec {
    struct { double r; } circle;
    struct { double w, h; } rect;
    struct {} dot;
};
using Shape = adt::sum<ShapeSpec>;
constexpr adt::make<ShapeSpec> shape{};

double area(const Shape& s) {
    return s.match<double>({
        .circle = [](const auto& c) { return 3.0 * c.r * c.r; },
        .rect = [](const auto& r) { return r.w * r.h; },
        .dot = [](const auto&) { return 0.0; },
    });
}

// rust: enum Expr { Num(f64), Add(Box<Expr>, Box<Expr>), Mul(Box<Expr>, Box<Expr>), Neg(Box<Expr>) }
struct ExprSpec;
using Expr = adt::sum<ExprSpec>;
using Box = std::unique_ptr<Expr>;
struct ExprSpec {
    double num;
    struct { Box lhs, rhs; } add;
    struct { Box lhs, rhs; } mul;
    struct { Box arg; } neg;
};
constexpr adt::make<ExprSpec> expr{};
Box box(Expr e) { return std::make_unique<Expr>(std::move(e)); }

double eval(const Expr& e) {
    return e.match<double>({
        .num = [](const double& n) { return n; },
        .add = [](const auto& a) { return eval(*a.lhs) + eval(*a.rhs); },
        .mul = [](const auto& m) { return eval(*m.lhs) * eval(*m.rhs); },
        .neg = [](const auto& n) { return -eval(*n.arg); },
    });
}

std::string show(const Expr& e) {
    return e.match<std::string>({
        .num = [](const double& n) { return std::format("{}", n); },
        .add = [](const auto& a) { return "(" + show(*a.lhs) + " + " + show(*a.rhs) + ")"; },
        .mul = [](const auto& m) { return "(" + show(*m.lhs) + " * " + show(*m.rhs) + ")"; },
        .neg = [](const auto& n) { return "-" + show(*n.arg); },
    });
}

// the arms a match must have, checked without compiling a failing call
inline constexpr auto zero = [](const auto&) { return 0.0; };
inline constexpr auto takes_int = [](const int&) { return 0.0; };

template <class S>
concept every_arm = requires(const S& s) { s.template match<double>({.circle = zero, .rect = zero, .dot = zero}); };
template <class S>
concept missing_dot = requires(const S& s) { s.template match<double>({.circle = zero, .rect = zero}); };
template <class S>
concept misspelled_dot = requires(const S& s) { s.template match<double>({.circle = zero, .rect = zero, .dott = zero}); };
template <class S>
concept wrong_payload = requires(const S& s) { s.template match<double>({.circle = takes_int, .rect = zero, .dot = zero}); };

int main() {
    std::vector<Shape> shapes;
    shapes.push_back(shape.circle({1.0}));
    shapes.push_back(shape.rect({2.0, 3.5}));
    shapes.push_back(shape.dot());
    for (const Shape& s : shapes) std::println("{}: area {}", s.name(), area(s));

    Shape copy = shapes[1];
    copy = shapes[0];
    std::println("copy is a {} with area {}", copy.name(), area(copy));

    Expr e = expr.add({box(expr.num(1)), box(expr.mul({box(expr.num(2)), box(expr.neg({box(expr.num(3))}))}))});
    std::println("{} = {}", show(e), eval(e));
    std::println("active alternative: {}", e.name());
    Expr moved = std::move(e);
    std::println("moved: {} = {}", show(moved), eval(moved));

    std::println("every arm: {}", every_arm<Shape>);
    std::println("missing dot: {}", missing_dot<Shape>);
    std::println("misspelled dot: {}", misspelled_dot<Shape>);
    std::println("wrong payload: {}", wrong_payload<Shape>);
}
