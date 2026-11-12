I want the vertex attribute table for a GPU vertex buffer (location, byte offset, component count and component type of each attribute) computed from the vertex struct itself, instead of being written by hand next to it, as for a call like `glVertexAttribPointer`. The table should be a constant, and a struct that cannot be described should give a readable reason.

Everything goes in `namespace vtx`. These two types are part of the API and must be defined exactly like this:

```cpp
enum class component { i8, u8, i16, u16, i32, u32, f32, f64 };

struct attribute {
    std::string_view name;
    unsigned location;
    std::size_t offset;
    unsigned count;
    component type;
    bool normalized;
};
```

Annotations, used with `[[=...]]` on data members exactly as in the test:
- `vtx::location(n)`: an explicit location, with an `unsigned` (or an `int` literal).
- `vtx::normalized`: a constant; the attribute is normalized.
- `vtx::skip`: a constant; the member is not an attribute.

API:
- `template <class T> constexpr std::array<vtx::attribute, N> vtx::layout`: a variable template with one entry per attribute of `T`, in declaration order, usable in constant expressions. Naming it for a `T` whose layout has an error must not compile.
- `template <class T> consteval std::string_view vtx::layout_error()` (`constexpr` is fine too): an empty string when `T` has a valid layout, otherwise the message for the first error, as described below.

Attributes:
- The attributes of `T` are its non-static data members without `vtx::skip`, in declaration order. Skipped members are not checked at all, but they still take up space, so they affect the offsets of later members. The test only uses public members.
- `name` is the member's name, `offset` its byte offset in `T`, and `normalized` is true exactly when the member has `vtx::normalized`.
- `type` and `count` depend on the member's type, ignoring `const` and `volatile` on the member type or its elements. A scalar is one of `std::int8_t`, `std::uint8_t`, `std::int16_t`, `std::uint16_t`, `std::int32_t`, `std::uint32_t`, `float` and `double`, mapped to `i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `f32` and `f64`. Aliases stand for the type they name, so on our platform `signed char` is `std::int8_t` and `int` is `std::int32_t`.
  - a scalar: its component, count 1
  - a one-dimensional C array `E[n]` of a scalar `E`: `E`'s component, count `n`
  - `std::array<E, n>` of a scalar `E`: `E`'s component, count `n`
  - a class type with at least one non-static data member, all of the same scalar type `E`, whose size is exactly the number of members times the size of `E` (so no padding, as in `vec3`): `E`'s component, count the number of members
  - any other type is unsupported.
- Locations: an attribute with `vtx::location(n)` has location `n`. An attribute without it has location 0 if it is the first attribute, and otherwise the location of the previous attribute plus one.

Layout errors, for `layout_error`:
- `<T>` below is the unqualified name of `T`. The attributes are checked in declaration order, and for each attribute the checks below are made in this order. The first check that fails gives the message.
  1. The member type is unsupported: `<T>.<member>: unsupported type`.
  2. The count is not between 1 and 4: `<T>.<member>: <count> components, need 1 to 4`.
  3. An earlier attribute has the same location: `<T>.<member>: location <location> already used`.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <print>
#include <string_view>

struct vec3 {
    float x, y, z;
};

struct rgba8 {
    std::uint8_t r, g, b, a;
};

struct Vertex {
    vec3 position;
    std::array<float, 2> uv;
    [[=vtx::normalized]] rgba8 color;
    [[=vtx::skip]] std::uint32_t cpu_flags;
    [[=vtx::location(6)]] std::int16_t bones[2];
    [[=vtx::normalized]] std::uint16_t weight;
    double time;
};

namespace fx {
struct Particle {
    [[=vtx::location(3)]] const float origin[3];
    int seed;
    [[=vtx::skip]] float transform[16];
    [[=vtx::location(1)]] std::array<std::uint32_t, 1> tint;
    signed char spin;
};
}  // namespace fx

// layouts with errors
struct Matrix {
    float m[16];
};

struct Flag {
    std::uint8_t level;
    bool on;
};

struct alignas(16) vec3a {
    float x, y, z;
};

struct Padded {
    vec3a position;
};

struct Mixed {
    struct {
        float f;
        std::int32_t i;
    } pair;
};

struct Wide {
    std::int64_t id;
};

struct Clash {
    float a[2];
    [[=vtx::location(2)]] float b;
    [[=vtx::location(1)]] float c;
    float d;
};

static_assert(vtx::layout<Vertex>.size() == 6);
static_assert(vtx::layout<Vertex>[3].offset == offsetof(Vertex, bones));
static_assert(vtx::layout<Vertex>[5].type == vtx::component::f64);
static_assert(vtx::layout<fx::Particle>[3].count == 1);

constexpr std::array<std::string_view, 8> type_names{"i8", "u8", "i16", "u16", "i32", "u32", "f32", "f64"};

template <class T>
void show(std::string_view title) {
    std::println("{} (stride {})", title, sizeof(T));
    for (const vtx::attribute& a : vtx::layout<T>)
        std::println("  {:<9} location {} offset {:>2} {}x{}{}", a.name, a.location, a.offset, a.count,
                     type_names[static_cast<std::size_t>(a.type)], a.normalized ? " normalized" : "");
}

template <class T>
void report(std::string_view title) {
    constexpr std::string_view error = vtx::layout_error<T>();
    std::println("{}: {}", title, error.empty() ? "ok" : error);
}

int main() {
    show<Vertex>("Vertex");
    show<fx::Particle>("Particle");
    report<Vertex>("Vertex");
    report<Matrix>("Matrix");
    report<Flag>("Flag");
    report<Padded>("Padded");
    report<Mixed>("Mixed");
    report<Wide>("Wide");
    report<Clash>("Clash");
}
```

It must print exactly:

```
Vertex (stride 48)
  position  location 0 offset  0 3xf32
  uv        location 1 offset 12 2xf32
  color     location 2 offset 20 4xu8 normalized
  bones     location 6 offset 28 2xi16
  weight    location 7 offset 32 1xu16 normalized
  time      location 8 offset 40 1xf64
Particle (stride 88)
  origin    location 3 offset  0 3xf32
  seed      location 4 offset 12 1xi32
  tint      location 1 offset 80 1xu32
  spin      location 2 offset 84 1xi8
Vertex: ok
Matrix: Matrix.m: 16 components, need 1 to 4
Flag: Flag.on: unsupported type
Padded: Padded.position: unsupported type
Mixed: Mixed.pair: unsupported type
Wide: Wide.id: unsupported type
Clash: Clash.d: location 2 already used
```

Write the complete header to `solution.hpp` in the current directory.
