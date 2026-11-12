I want Rust-style enums with data in C++. The user lists the alternatives as the members of a plain spec struct, builds values with constructors named after the alternatives, and takes them apart with a `match` that has one named arm per alternative. A `match` that forgets an alternative must not compile.

Everything goes in `namespace adt`. In a spec struct, each non-static data member is one alternative: its name is the alternative's name and its type is the alternative's payload type. The alternatives are ordered by declaration.
- `adt::sum<Spec>`: a class template holding exactly one alternative of `Spec` together with its payload. It is copyable when every payload is copyable and movable when every payload is movable (the `Expr` payloads hold `std::unique_ptr`). `Spec` may be incomplete where `sum<Spec>` is first named, as with `ExprSpec`.
- `adt::make<Spec>`: an aggregate type, usable as a `constexpr` variable initialized with `{}`, with one member per alternative, of the same name and in the same order. Calling that member with a payload, as `shape.circle({1.0})`, returns a `sum<Spec>` holding that alternative and payload. The argument may be left out, as in `shape.dot()`; the payload is then value-initialized (`{}`).
- `template <class R> R sum<Spec>::match(arms) const`: calls the arm for the active alternative with a `const` reference to its payload and returns what it returns. Exactly one arm is called.
- `std::string_view sum<Spec>::name() const`: the name of the active alternative.

The argument of `match`:
- It is a braced list of designated initializers, one per alternative, named after the alternatives and in the order of `Spec`, as in the test. Each arm is a callable, typically a lambda, that can be called with a `const P&` for that alternative's payload type `P` and returns something convertible to `R`.
- `match` does not own or copy the arms: they live until the end of the full expression that contains the call, which is all it needs.
- A call that leaves out an arm, names an arm that is not an alternative, or gives an arm that cannot be called with that alternative's payload must not compile. It must fail as part of the call expression itself, during overload resolution, and not later inside the body of `match`, so that a requires-expression containing such a call is `false`. The test checks this with concepts.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
circle: area 3
rect: area 7
dot: area 0
copy is a circle with area 3
(1 + (2 * -3)) = -5
active alternative: add
moved: (1 + (2 * -3)) = -5
every arm: true
missing dot: false
misspelled dot: false
wrong payload: false
```

Write the complete header to `solution.hpp` in the current directory.
