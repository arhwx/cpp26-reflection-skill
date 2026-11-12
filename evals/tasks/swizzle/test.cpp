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
