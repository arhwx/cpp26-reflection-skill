#include "solution.hpp"

#include <format>
#include <memory>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>

namespace shapes {
struct Shape {
    virtual ~Shape() = default;
};
struct Circle : Shape {
    explicit Circle(double r) : r(r) {}
    double r;
};
struct Square : Shape {
    explicit Square(double side) : side(side) {}
    double side;
};
struct RoundedSquare : Square {
    RoundedSquare(double side, double corner) : Square(side), corner(corner) {}
    double corner;
};
struct Triangle : Shape {
    Triangle(double base, double height) : base(base), height(height) {}
    double base, height;
};

// not cases of Shape
struct Color {
    int rgb;
};
using Box = Square;
enum class Kind { flat, solid };
inline double scale = 1.0;

// a second hierarchy in the same namespace
struct Pen {
    virtual ~Pen() = default;
};
struct Brush : Pen {};
}  // namespace shapes

namespace plugins {
struct Hexagon : shapes::Shape {};
}  // namespace plugins

using Area = sealed::visitor<^^shapes, shapes::Shape, double>;
using Describe = sealed::visitor<^^shapes, shapes::Shape, std::string>;
using Print = sealed::visitor<^^shapes, shapes::Shape, void>;

inline constexpr auto any = [](const shapes::Shape&) { return 0.0; };
inline constexpr auto ignore = [](const auto&) { return 0.0; };
inline constexpr auto square_only = [](const shapes::Square&) { return 0.0; };

template <class V>
concept all_cases = requires { V{.circle = any, .rounded_square = any, .square = any, .triangle = any}; };
template <class V>
concept missing_triangle = requires { V{.circle = any, .rounded_square = any, .square = any}; };
template <class V>
concept with_box = requires { V{.box = any, .circle = any, .rounded_square = any, .square = any, .triangle = any}; };
template <class V>
concept wrong_arm = requires { V{.circle = square_only, .rounded_square = any, .square = any, .triangle = any}; };
template <class V>
concept unsorted = requires { V{.circle = any, .square = any, .rounded_square = any, .triangle = any}; };
template <class V>
concept only_brush = requires { V{.brush = ignore}; };
template <class V>
concept only_rounded = requires { V{.rounded_square = square_only}; };

int main() {
    std::vector<std::unique_ptr<shapes::Shape>> all;
    all.push_back(std::make_unique<shapes::Circle>(1.0));
    all.push_back(std::make_unique<shapes::Square>(2.0));
    all.push_back(std::make_unique<shapes::RoundedSquare>(3.0, 0.5));
    all.push_back(std::make_unique<shapes::Triangle>(4.0, 1.5));

    const Area area{
        .circle = [](const shapes::Circle& c) { return 3.0 * c.r * c.r; },
        .rounded_square = [](const shapes::RoundedSquare& s) { return s.side * s.side - s.corner; },
        .square = [](const shapes::Square& s) { return s.side * s.side; },
        .triangle = [](const shapes::Triangle& t) { return t.base * t.height / 2; },
    };
    const Describe describe{
        .circle = [](const auto& c) { return std::format("circle r={}", c.r); },
        .rounded_square = [](const shapes::Square& s) { return std::format("rounded square side={}", s.side); },
        .square = [](const auto& s) { return std::format("square side={}", s.side); },
        .triangle = [](const shapes::Shape&) { return std::string("some triangle"); },
    };
    int printed = 0;
    const Print print{
        .circle = [&](const shapes::Circle&) { ++printed; std::println("  print circle"); },
        .rounded_square = [&](const shapes::RoundedSquare&) { ++printed; return std::println("  print rounded"); },
        .square = [&](const shapes::Square&) { ++printed; std::println("  print square"); },
        .triangle = [&](const auto&) { ++printed; std::println("  print triangle"); return 42; },
    };

    for (const auto& s : all) {
        double a = sealed::visit(*s, area);
        std::string d = sealed::visit(*s, describe);
        std::println("{}: area {}", d, a);
    }
    for (const auto& s : all) sealed::visit(*s, print);
    std::println("printed {}", printed);

    const shapes::Square& as_square = static_cast<const shapes::Square&>(*all[2]);
    std::println("through Square&: {}", sealed::visit(as_square, describe));

    plugins::Hexagon hex;
    try {
        sealed::visit(static_cast<const shapes::Shape&>(hex), area);
    } catch (const std::logic_error& e) {
        std::println("hexagon: {}", e.what());
    }
    shapes::Shape plain;
    try {
        std::println("plain: {}", sealed::visit(plain, describe));
    } catch (const std::logic_error& e) {
        std::println("plain shape: {}", e.what());
    }

    shapes::Brush brush;
    const sealed::visitor<^^shapes, shapes::Pen, int> pens{.brush = [](const shapes::Brush&) { return 7; }};
    std::println("brush: {}", sealed::visit(static_cast<const shapes::Pen&>(brush), pens));

    std::println("all cases: {}", all_cases<Area>);
    std::println("missing triangle: {}", missing_triangle<Area>);
    std::println("alias as case: {}", with_box<Area>);
    std::println("wrong arm: {}", wrong_arm<Area>);
    std::println("unsorted: {}", unsorted<Area>);
    std::println("pens: brush only: {}", only_brush<sealed::visitor<^^shapes, shapes::Pen, double>>);
    std::println("shapes as pens: {}", only_brush<Area>);
    std::println("below Square: {}", only_rounded<sealed::visitor<^^shapes, shapes::Square, double>>);
}
