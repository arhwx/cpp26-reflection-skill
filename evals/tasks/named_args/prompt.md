I want named arguments for ordinary C++ functions: `named::invoke<^^draw>({.width = 3, .height = 4})`. The argument struct should be generated from the function's own parameter names and types, so nothing is written per function.

API, in namespace `named`:
- `template <std::meta::info F> using args = ...;`: an aggregate type with one public data member per parameter of `F`, named like the parameter, in parameter order. It is filled with designated initializers and can also be stored in a variable.
- `template <std::meta::info F> R invoke(args<F> a)`: calls `F` with the arguments in `a` and returns its result. `R` is `F`'s return type, which may be `void`. `F` reflects a free function (also in a namespace) or a static member function.
- `template <std::meta::info F, class Obj> R invoke(Obj& obj, args<F> a)`: the same for a non-static member function `F`, called on `obj`.
- Each `invoke` overload only takes part in overload resolution for its own kind of function: calling a non-static member function without an object, or a static member function with one, is ill-formed in a way a requires-expression detects (it yields `false`, it is not a hard error).
- `invoke` takes `args<F>` by value, so a stored `args<F>` object can be passed more than once.

Members and initialization:
- A parameter of type `T&` (non-const lvalue reference) gets a member that binds to the object given, so the function reads and changes the caller's object. Only an lvalue of type `T` can initialize it, and it must always be given.
- Every other parameter is taken by value or by const reference. Its member holds a value of the parameter type with reference and cv-qualifiers removed. Any expression implicitly convertible to that type can initialize it.
- A parameter without a default argument must be given. Leaving it out, misspelling a name, or giving a value that is not implicitly convertible makes the braced initialization of `args<F>` ill-formed, so a requires-expression around such a call yields `false`.
- A parameter with a default argument may be left out. `invoke` passes the leading run of given arguments and leaves out the rest, so `F` uses its own default arguments for them.
- If a parameter with a default argument is left out but a later parameter is given, `invoke` does not call `F` and throws `std::invalid_argument` whose `what()` is `<function>: <left out> must be given because <later> is given`. `<function>` is the function's unqualified name, `<left out>` the first parameter that was left out, and `<later>` the first given parameter after it.

The test functions name every parameter, and no parameter with a default argument is a non-const reference.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <stdexcept>
#include <string>
#include <vector>

double area(double width, double height) { return width * height; }

std::string greet(const std::string& name, int times = 1, bool shout = false) {
    std::string text = name;
    for (int i = 1; i < times; ++i)
        text += " " + name;
    return shout ? text + "!" : text;
}

// appends count values to out
void fill(std::vector<int>& out, int count, int start = 0, int step = 1) {
    for (int i = 0; i < count; ++i)
        out.push_back(start + i * step);
}

int answer() { return 42; }

namespace geo {
struct Size {
    int w;
    int h;
};
Size scaled(Size size, int factor = 2) { return {size.w * factor, size.h * factor}; }
}  // namespace geo

class Canvas {
public:
    std::string draw(const std::string& shape, int size = 1, char ink = '*') {
        ++strokes;
        return shape + ":" + std::string(size, ink);
    }
    static int clamp(int value, int lo = 0, int hi = 100) { return value < lo ? lo : value > hi ? hi : value; }

    int strokes = 0;
};

std::string show(const std::vector<int>& values) {
    std::string out;
    for (int v : values)
        out += (out.empty() ? "" : " ") + std::to_string(v);
    return "[" + out + "]";
}

template <class F>
void attempt(const char* label, F f) {
    try {
        std::println("{}: {}", label, f());
    } catch (const std::invalid_argument& e) {
        std::println("{}: error: {}", label, e.what());
    }
}

template <bool B>
void check(const char* label) {
    std::println("{}: {}", label, B);
}

template <auto F>
concept area_ok = requires { named::invoke<F>({.width = 1.0, .height = 2.0}); };
template <auto F>
concept area_no_height = requires { named::invoke<F>({.width = 1.0}); };
template <auto F>
concept area_misspelled = requires { named::invoke<F>({.width = 1.0, .hieght = 2.0}); };
template <auto F>
concept area_text = requires { named::invoke<F>({.width = "wide", .height = 2.0}); };
template <auto F>
concept greet_no_name = requires { named::invoke<F>({.times = 2}); };
template <auto F>
concept fill_lvalue = requires(std::vector<int>& v) { named::invoke<F>({.out = v, .count = 1}); };
template <auto F>
concept fill_temporary = requires { named::invoke<F>({.out = std::vector<int>{}, .count = 1}); };
template <auto F>
concept without_object = requires { named::invoke<F>({.shape = "x"}); };
template <auto F>
concept with_object = requires(Canvas& c) { named::invoke<F>(c, {.value = 1}); };

int main() {
    std::println("area: {}", named::invoke<^^area>({.width = 3, .height = 4.5}));

    attempt("greet", [] { return named::invoke<^^greet>({.name = "ann"}); });
    attempt("greet times", [] { return named::invoke<^^greet>({.name = "ann", .times = 3}); });
    attempt("greet all", [] { return named::invoke<^^greet>({.name = "ann", .times = 2, .shout = true}); });
    attempt("greet gap", [] { return named::invoke<^^greet>({.name = "ann", .shout = true}); });

    std::vector<int> values;
    named::invoke<^^fill>({.out = values, .count = 3, .start = 5, .step = 2});
    named::invoke<^^fill>({.out = values, .count = 2});
    std::println("fill: {}", show(values));
    attempt("fill gap", [&] {
        named::invoke<^^fill>({.out = values, .count = 1, .step = 3});
        return show(values);
    });
    std::println("fill after gap: {}", show(values));

    std::println("answer: {}", named::invoke<^^answer>({}));

    geo::Size big = named::invoke<^^geo::scaled>({.size = geo::Size{2, 3}});
    geo::Size huge = named::invoke<^^geo::scaled>({.size = big, .factor = 10});
    std::println("scaled: {}x{} {}x{}", big.w, big.h, huge.w, huge.h);

    Canvas canvas;
    attempt("draw", [&] { return named::invoke<^^Canvas::draw>(canvas, {.shape = "box", .size = 3}); });
    attempt("draw ink", [&] { return named::invoke<^^Canvas::draw>(canvas, {.shape = "dot", .size = 2, .ink = 'o'}); });
    attempt("draw gap", [&] { return named::invoke<^^Canvas::draw>(canvas, {.shape = "dot", .ink = 'o'}); });
    std::println("strokes: {}", canvas.strokes);
    std::println("clamp: {} {} {}", named::invoke<^^Canvas::clamp>({.value = 140}),
                 named::invoke<^^Canvas::clamp>({.value = -5, .lo = -3}),
                 named::invoke<^^Canvas::clamp>({.value = 70, .lo = 0, .hi = 50}));
    attempt("clamp gap", [] { return named::invoke<^^Canvas::clamp>({.value = 7, .hi = 5}); });

    named::args<^^area> saved{.width = 2, .height = 5};
    std::println("saved area: {} {}", named::invoke<^^area>(saved), named::invoke<^^area>(saved));

    check<area_ok<^^area>>("area ok");
    check<area_no_height<^^area>>("area without height");
    check<area_misspelled<^^area>>("area misspelled");
    check<area_text<^^area>>("area text width");
    check<greet_no_name<^^greet>>("greet without name");
    check<fill_lvalue<^^fill>>("fill lvalue");
    check<fill_temporary<^^fill>>("fill temporary");
    check<without_object<^^Canvas::draw>>("draw without object");
    check<with_object<^^Canvas::clamp>>("clamp with object");
}
```

It must print exactly:

```
area: 13.5
greet: ann
greet times: ann ann ann
greet all: ann ann!
greet gap: error: greet: times must be given because shout is given
fill: [5 7 9 0 1]
fill gap: error: fill: start must be given because step is given
fill after gap: [5 7 9 0 1]
answer: 42
scaled: 4x6 40x60
draw: box:***
draw ink: dot:oo
draw gap: error: draw: size must be given because ink is given
strokes: 2
clamp: 100 -3 50
clamp gap: error: clamp: lo must be given because hi is given
saved area: 10 10
area ok: true
area without height: false
area misspelled: false
area text width: false
greet without name: false
fill lvalue: true
fill temporary: false
draw without object: false
clamp with object: false
```

Write the complete header to `solution.hpp` in the current directory.
