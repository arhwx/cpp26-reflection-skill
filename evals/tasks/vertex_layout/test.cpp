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
