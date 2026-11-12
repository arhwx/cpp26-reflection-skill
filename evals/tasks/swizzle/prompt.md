I want GLSL-style small vectors in C++ where the component names are real data members chosen by a string: `glsl::vec<float, "xyz">` has members `x`, `y` and `z`, and `glsl::vec<unsigned char, "rgba">` has `r`, `g`, `b` and `a`. On top of that I want GLSL swizzles, read as `v.sw<"zyx">()` and written as `v.assign<"zx">(other)`.

Everything goes in `namespace glsl`.
- `glsl::vec<T, "xyz">`: a class template whose second template argument is a string literal, the names. Each character is one component name; the characters are distinct letters. `T` is an arithmetic type.
- `vec<T, Names>` is an aggregate with exactly one public non-static data member of type `T` per name, called by that one-letter name, in the order of the string, and no other data. `sizeof` is the number of names times `sizeof(T)`, and it is trivially copyable. A braced list such as `vec3{1, 2, 3}` initializes the components in order.
- `static constexpr std::size_t size`: the number of names.
- `operator[](std::size_t i)`, `const` and non-`const`: a reference to component `i` (0-based, in name order). The test only uses valid indices.
- `v.sw<"zyx">()`: a `constexpr`, `const` member function template whose template argument `S` is a string literal of `k` letters. Each letter must be one of this vector's names, letters may repeat, and `1 <= k <= size`. When `k` is 1 the result is that component, as a `T`. Otherwise the result is a `glsl::vec<T, P>`, where `P` is the first `k` names of this vector (so on an `"xyzw"` vector, `sw<"wzy">()` is a `vec<T, "xyz">`), and its component `j` is this vector's component named by letter `j` of `S`.
- `v.assign<"zx">(other)`: a `constexpr` member function template returning `vec&`, whose template argument `S` is a string literal of `k` distinct letters, each one of this vector's names, with `1 <= k <= size`, and `other` is a `glsl::vec<T, O>` with the same `T` and exactly `k` names (any names). Sets this vector's component named by letter `j` of `S` to component `j` of `other`, for every `j`. It works as if `other` were copied first, so `s.assign<"yx">(s)` swaps. Returns `*this`.
- A `sw` or `assign` that breaks these rules must not be callable: a requires-expression that calls it is `false`, as the test checks.
- `a + b` and `a - b` for two vectors of the same type: componentwise. `v * k` and `k * v` with `k` of type `T`: every component multiplied by `k`. `a == b` for the same type: true when every component is equal. `dot(a, b)`: the sum of the componentwise products, of type `T`. These are all usable in constant expressions; `sw`, `assign` and `operator[]` are too, as the test shows.
- `str(v)` returns a `std::string`: `{`, then `<name>: <value>` for each component in order, separated by `, `, then `}`. Values are written with `std::format("{}", value)`.
- The test calls `dot` and `str` without qualification.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <type_traits>

using vec2 = glsl::vec<float, "xy">;
using vec3 = glsl::vec<float, "xyz">;
using vec4 = glsl::vec<float, "xyzw">;
using rgba = glsl::vec<unsigned char, "rgba">;
using ijk = glsl::vec<int, "ijk">;

template <class V>
concept read_xxx = requires(const V& v) { v.template sw<"xxx">(); };
template <class V>
concept read_xxxx = requires(const V& v) { v.template sw<"xxxx">(); };
template <class V>
concept read_xq = requires(const V& v) { v.template sw<"xq">(); };
template <class V>
concept read_z = requires(const V& v) { v.template sw<"z">(); };
template <class V>
concept read_empty = requires(const V& v) { v.template sw<"">(); };
template <class V, class A>
concept write_xy = requires(V& v, const A& a) { v.template assign<"xy">(a); };
template <class V, class A>
concept write_xx = requires(V& v, const A& a) { v.template assign<"xx">(a); };

int main() {
    vec3 v{1, 2, 3};
    v.y = 20;
    vec3 w = v + vec3{10, 10, 10};
    std::println("v = {}", str(v));
    std::println("w = {}", str(w));
    std::println("w - v = {}", str(w - v));
    std::println("v * 0.5 = {}", str(v * 0.5f));
    std::println("2 * v = {}", str(2.0f * v));
    std::println("dot(v, w) = {}", dot(v, w));
    v[2] = 30;
    const vec3 cv = v;
    std::println("v[0] = {}, cv[2] = {}", v[0], cv[2]);

    auto zyx = v.sw<"zyx">();
    auto xx = v.sw<"xx">();
    auto z = v.sw<"z">();
    std::println("zyx = {}, is vec3: {}", str(zyx), std::is_same_v<decltype(zyx), vec3>);
    std::println("xx = {}, is vec2: {}", str(xx), std::is_same_v<decltype(xx), vec2>);
    std::println("z = {}, is float: {}", z, std::is_same_v<decltype(z), float>);
    std::println("chained = {}", str(v.sw<"zyx">().sw<"yz">()));
    std::println("v == v: {}, v == zyx: {}", v == v, v == zyx);

    vec4 p{1, 2, 3, 4};
    auto wzy = p.sw<"wzy">();
    std::println("wzy = {}, is vec3: {}", str(wzy), std::is_same_v<decltype(wzy), vec3>);
    p.assign<"wx">(vec2{9, 8});
    std::println("p = {}", str(p));
    p.assign<"zyx">(v).assign<"wy">(wzy.sw<"xy">());
    std::println("p = {}", str(p));
    vec2 s{1, 2};
    s.assign<"yx">(s);
    std::println("swapped = {}", str(s));

    rgba c{255, 128, 0, 255};
    auto bgr = c.sw<"bgr">();
    std::println("bgr = {}", str(bgr));
    std::println("bgr is rgb: {}", std::is_same_v<decltype(bgr), glsl::vec<unsigned char, "rgb">>);
    c.assign<"rg">(c.sw<"gr">());
    std::println("c = {}, size {}", str(c), rgba::size);

    ijk n{1, 2, 3};
    std::println("kji = {}", str(n.sw<"kji">()));
    std::println("n.k = {}", n.k);

    constexpr float first = vec3{1, 2, 3}.sw<"zx">()[0];
    static_assert(vec3{1, 2, 3}.sw<"zx">().y == 1);
    std::println("constexpr first = {}", first);

    std::println("sizeof vec4 is 4 floats: {}", sizeof(vec4) == 4 * sizeof(float));
    std::println("aggregate: {}, trivially copyable: {}", std::is_aggregate_v<vec3>,
                 std::is_trivially_copyable_v<vec3>);
    std::println("member types: {} {}", std::is_same_v<decltype(v.x), float>,
                 std::is_same_v<decltype(c.a), unsigned char>);

    std::println("vec3 reads xxx: {}", read_xxx<vec3>);
    std::println("vec3 reads xxxx: {}", read_xxxx<vec3>);
    std::println("vec4 reads xxxx: {}", read_xxxx<vec4>);
    std::println("vec3 reads xq: {}", read_xq<vec3>);
    std::println("vec3 reads z: {}", read_z<vec3>);
    std::println("vec2 reads z: {}", read_z<vec2>);
    std::println("vec3 reads empty: {}", read_empty<vec3>);
    std::println("vec3 writes xy from vec2: {}", write_xy<vec3, vec2>);
    std::println("vec3 writes xy from vec3: {}", write_xy<vec3, vec3>);
    std::println("vec3 writes xx from vec2: {}", write_xx<vec3, vec2>);
    std::println("vec3 writes xy from ijk: {}", write_xy<vec3, glsl::vec<int, "ij">>);
}
```

It must print exactly:

```
v = {x: 1, y: 20, z: 3}
w = {x: 11, y: 30, z: 13}
w - v = {x: 10, y: 10, z: 10}
v * 0.5 = {x: 0.5, y: 10, z: 1.5}
2 * v = {x: 2, y: 40, z: 6}
dot(v, w) = 650
v[0] = 1, cv[2] = 30
zyx = {x: 30, y: 20, z: 1}, is vec3: true
xx = {x: 1, y: 1}, is vec2: true
z = 30, is float: true
chained = {x: 20, y: 1}
v == v: true, v == zyx: false
wzy = {x: 4, y: 3, z: 2}, is vec3: true
p = {x: 8, y: 2, z: 3, w: 9}
p = {x: 30, y: 3, z: 1, w: 4}
swapped = {x: 2, y: 1}
bgr = {r: 0, g: 128, b: 255}
bgr is rgb: true
c = {r: 128, g: 255, b: 0, a: 255}, size 4
kji = {i: 3, j: 2, k: 1}
n.k = 3
constexpr first = 3
sizeof vec4 is 4 floats: true
aggregate: true, trivially copyable: true
member types: true true
vec3 reads xxx: true
vec3 reads xxxx: false
vec4 reads xxxx: true
vec3 reads xq: false
vec3 reads z: true
vec2 reads z: false
vec3 reads empty: false
vec3 writes xy from vec2: true
vec3 writes xy from vec3: false
vec3 writes xx from vec2: false
vec3 writes xy from ijk: false
```

Write the complete header to `solution.hpp` in the current directory.
