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
