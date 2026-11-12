I want Kotlin-style sealed hierarchies with generated visitors. All the classes of a hierarchy live in one namespace; a visitor type is generated from that namespace with one named member per class, and a visitor that forgets a class must not compile. Adding a class to the namespace must not require touching the library or any list.

Everything goes in `namespace sealed`.

The cases of `Base` in namespace `ns` are the classes declared directly in `ns` (not in a nested or another namespace) that have `Base` as a base class, directly or indirectly, other than `Base` itself. Type aliases, enums, variables and functions are never cases, even when an alias names a case class. The test only has named, complete, non-template classes in the namespace.

API:
- `sealed::visitor<Ns, Base, R>`: `Ns` is a reflection of a namespace (written `^^shapes` in the test), `Base` is a class type and `R` is a type, possibly `void`. It is an aggregate type with one public data member per case of `Base` in that namespace, and no other members.
  - The member's name is the class name in snake case: every uppercase ASCII letter is replaced by its lowercase letter, preceded by `_` unless it is the first character of the name. Other characters are kept. So `RoundedSquare` gives `rounded_square`.
  - The members are ordered by name, in ascending byte order of the member names (as `std::string_view` comparison). Designated initializers must follow this order.
  - The member for class `C` can be initialized from any callable object `f` (a lambda in the test) such that `f(c)`, for an lvalue `c` of type `const C`, is valid and converts to `R`; when `R` is `void`, any return type is accepted and the result is ignored. The visitor keeps its own copy of the callable.
  - Each member type has no default constructor, and initializing it from a callable that does not meet the rule above must fail during overload resolution. So a visitor initializer that leaves out a member, names something that is not a member, or gives a member a callable of the wrong kind is ill-formed in a way a requires-expression detects (`false`). The test checks this with concepts.
  - Visitor objects are copyable and usable as `const` objects.
- `template <class B, class V> R sealed::visit(const B& obj, const V& v)`: `V` is some `visitor<Ns, Base, R>` and `B` is `Base` or a class derived from it, with `Base` polymorphic. It finds the case class `C` that is exactly the dynamic type of `obj`, calls the member of `v` for `C` once with `obj` as a `const C&`, and returns the result converted to `R` (nothing for `void`). Every other member is not called.
  - If the dynamic type of `obj` is not a case (for example a class derived from `Base` in another namespace, or `Base` itself), `visit` calls no member and throws `std::logic_error` whose `what()` is `unhandled dynamic type`.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
circle r=1: area 3
square side=2: area 4
rounded square side=3: area 8.5
some triangle: area 3
  print circle
  print square
  print rounded
  print triangle
printed 4
through Square&: rounded square side=3
hexagon: unhandled dynamic type
plain shape: unhandled dynamic type
brush: 7
all cases: true
missing triangle: false
alias as case: false
wrong arm: false
unsorted: false
pens: brush only: true
shapes as pens: false
below Square: true
```

Write the complete header to `solution.hpp` in the current directory.
