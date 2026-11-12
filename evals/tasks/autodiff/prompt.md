I want to write a small formula as a string and get a function with named arguments, together with its exact derivatives, all worked out by the compiler. A derivative is again such a function, so it can be printed, evaluated and differentiated further.

Everything goes in `namespace sym`:
- `sym::fn<"formula">`: a class type for the formula, which is given as a string literal template argument. Call this type `F` below.
- `F::args`: an aggregate struct with one public `double` member for each variable of the formula, named after the variable, in order of first appearance in the formula text from left to right. It has no other members, so it can be built with designated initializers as in the test.
- `static constexpr double F::eval(const F::args& a)`: the value of the formula at `a`.
- `static constexpr F::args F::grad(const F::args& a)`: an `F::args` whose every member holds the partial derivative with respect to that variable, evaluated at `a`.
- `static constexpr std::string_view F::text`: the formula as text, in the format below.
- `F::d<"name">`: a member template that takes a string literal. It names the type of the derivative of `F` with respect to the variable `name`, with the same members `args`, `eval`, `grad`, `text` and `d`. Its `args` is the same type as `F::args`, even when the derivative no longer contains some of the variables, so it can be differentiated again with respect to any variable of the original formula. A name that is not a member of `F::args` must not compile; the test never does this.
- `eval` and `grad` must be usable in constant expressions.

Formula syntax:
- A formula is made of numbers, variable names, `+`, `-`, `*`, parentheses and spaces. Spaces may appear before and after any token, also at the start and the end, and are ignored.
- A number is one or more decimal digits. A variable name is a lowercase letter followed by any number of lowercase letters and digits.
- `*` binds tighter than `+` and `-`. `+` and `-` have the same precedence. All three are left associative. Parentheses group as usual. There is no unary minus.
- A formula that does not follow this syntax must not compile; the test never does this.

Expression trees. A formula and each of its derivatives is a tree whose leaves are constants (a `double`) and variables, and whose inner nodes are `+`, `-` and `*` with two children. Two trees are identical when they have the same shape, the same operators at the same places, the same variables at the same places and equal constants at the same places. Trees are always made with the operation build below, never directly:
- While parsing, each time an operator has both of its operands, the result is build(left, op, right). Parentheses do not create a node of their own. So `x*x*y` is build(build(x, `*`, x), `*`, y), and the number `3` is the constant 3.
- build(l, op, r), where `l` and `r` are trees that were already built, uses the first rule that matches:
  1. `l` and `r` are both constants: the constant `l op r`, computed in `double`.
  2. `op` is `*` and `l` or `r` is the constant 0: the constant 0.
  3. `op` is `*` and `l` is the constant 1: `r`.
  4. `op` is `*` and `r` is the constant 1: `l`.
  5. `op` is `+` and `l` is the constant 0: `r`.
  6. `op` is `+` or `-` and `r` is the constant 0: `l`.
  7. `op` is `-` and `l` and `r` are identical: the constant 0.
  8. `op` is `+` and `l` and `r` are identical: build(constant 2, `*`, l).
  9. otherwise: a new node `op` with left child `l` and right child `r`.
- No other simplification happens. For example `2*3*u` becomes `(6 * u)`, but `u*2*3` stays `((u * 2) * 3)`.

Derivative D(e) of a tree `e` with respect to the variable `x`:
- a constant: the constant 0.
- a variable: the constant 1 if it is `x`, otherwise the constant 0.
- `l + r`: build(D(l), `+`, D(r)). `l - r`: build(D(l), `-`, D(r)).
- `l * r`: build(build(D(l), `*`, r), `+`, build(l, `*`, D(r))).

Text format:
- A constant is written as a whole number in decimal, without a decimal point, with a leading `-` when it is negative. The test only produces constants that are whole numbers.
- A variable is written as its name.
- A node is written as `(`, the left child, a space, the operator, a space, the right child, `)`.

`eval` computes the value of the tree with `double` arithmetic, reading each variable from the member of `args` with its name.

Types: every type reached from one `F` through `d`, any number of times (zero times is `F` itself), is the same type as another such type exactly when their trees are identical. So `F::d<"x">::d<"y">` and `F::d<"y">::d<"x">` are the same type when the two trees are identical, and two derivatives that are both the constant 0 are the same type.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
f        = ((((x * x) * y) + (3 * y)) - 7)
df/dx    = ((2 * x) * y)
df/dy    = ((x * x) + 3)
d2f/dx2  = (2 * y)
d2f/dxdy = (2 * x)
d3f/dx3  = 0
d2f/dy2  = 0
mixed partials are one type: true
zero derivatives are one type: true
df/dx and df/dy are one type: false
derivatives take the same args: true
f(2, 5) = 28, gradient = (20, 7)
hessian row x at (3, -1) = (-2, 6)
g      = ((((t * t) * t) - (2 * t)) - 5)
g'     = ((((2 * t) * t) + (t * t)) - 2)
g''    = ((2 * (2 * t)) + (2 * t))
g'''   = 6
g''''  = 0
root of g near 2: 2.094551
h      = ((-3 + (b * (a + 1))) + a)
dh/da  = (b + 1)
dh/db  = (a + 1)
h(a = 10, b = 4) = 51, gradient = (a: 5, b: 11)
k      = ((((2 * u) + (v * v)) + (6 * u)) + ((u * 2) * 3))
dk/du  = 14
dk/dv  = (2 * v)
k(1, 2) = 18
```

Write the complete header to `solution.hpp` in the current directory.
