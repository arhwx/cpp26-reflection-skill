#include "solution.hpp"

#include <print>
#include <type_traits>

using F = sym::fn<"x*x*y + 3*y - 7">;
using G = sym::fn<"t*t*t - 2*t - 5">;
using H = sym::fn<"2 - 5 + b*(a + 1) + 0*a + a*1 - (b - b)">;
using K = sym::fn<" u+u + v*v + 2*3*u + u*2*3 ">;

int main() {
    std::println("f        = {}", F::text);
    std::println("df/dx    = {}", F::d<"x">::text);
    std::println("df/dy    = {}", F::d<"y">::text);
    std::println("d2f/dx2  = {}", F::d<"x">::d<"x">::text);
    std::println("d2f/dxdy = {}", F::d<"x">::d<"y">::text);
    std::println("d3f/dx3  = {}", F::d<"x">::d<"x">::d<"x">::text);
    std::println("d2f/dy2  = {}", F::d<"y">::d<"y">::text);

    std::println("mixed partials are one type: {}", std::is_same_v<F::d<"x">::d<"y">, F::d<"y">::d<"x">>);
    std::println("zero derivatives are one type: {}", std::is_same_v<F::d<"x">::d<"x">::d<"x">, F::d<"y">::d<"y">>);
    std::println("df/dx and df/dy are one type: {}", std::is_same_v<F::d<"x">, F::d<"y">>);
    std::println("derivatives take the same args: {}", std::is_same_v<F::d<"x">::d<"x">::d<"x">::args, F::args>);

    constexpr double f25 = F::eval({.x = 2, .y = 5});
    constexpr F::args g = F::grad({.x = 2, .y = 5});
    std::println("f(2, 5) = {}, gradient = ({}, {})", f25, g.x, g.y);
    F::args h = F::d<"x">::grad({.x = 3, .y = -1});
    std::println("hessian row x at (3, -1) = ({}, {})", h.x, h.y);

    double t = 2;  // newton's method with the derivative the compiler worked out
    for (int i = 0; i < 6; ++i) t -= G::eval({.t = t}) / G::d<"t">::eval({.t = t});
    std::println("g      = {}", G::text);
    std::println("g'     = {}", G::d<"t">::text);
    std::println("g''    = {}", G::d<"t">::d<"t">::text);
    std::println("g'''   = {}", G::d<"t">::d<"t">::d<"t">::text);
    std::println("g''''  = {}", G::d<"t">::d<"t">::d<"t">::d<"t">::text);
    std::println("root of g near 2: {:.6f}", t);

    std::println("h      = {}", H::text);
    std::println("dh/da  = {}", H::d<"a">::text);
    std::println("dh/db  = {}", H::d<"b">::text);
    H::args hg = H::grad({.b = 4, .a = 10});
    std::println("h(a = 10, b = 4) = {}, gradient = (a: {}, b: {})", H::eval({.b = 4, .a = 10}), hg.a, hg.b);

    std::println("k      = {}", K::text);
    std::println("dk/du  = {}", K::d<"u">::text);
    std::println("dk/dv  = {}", K::d<"v">::text);
    std::println("k(1, 2) = {}", K::eval({.u = 1, .v = 2}));
}
