// swizzle: a vector type whose members are named by a string, like glsl. vec<float, "xyz"> has real
// members x, y and z, an index operator, and swizzles such as v.sw<"zyx">()
#include <cstddef>
#include <meta>
#include <print>
#include <ranges>
#include <string_view>
#include <utility>
#include <vector>

template <std::size_t N>
struct names {
    char s[N]{};
    consteval names(const char (&a)[N]) { for (std::size_t i = 0; i < N; ++i) s[i] = a[i]; }
    static constexpr std::size_t size = N - 1;
};

// storage: one real data member per letter, generated
template <class T, names Ns>
struct vec_storage {
    struct type;
    consteval {
        std::vector<std::meta::info> specs;
        for (std::size_t i = 0; i < Ns.size; ++i)
            specs.push_back(std::meta::data_member_spec(^^T, {.name = std::string_view(&Ns.s[i], 1)}));
        std::meta::define_aggregate(^^type, specs);
    }
};

template <class T, names Ns>
struct vec : vec_storage<T, Ns>::type {
    static constexpr std::size_t size = Ns.size;
    static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(
        ^^typename vec_storage<T, Ns>::type, std::meta::access_context::current()));

    // runtime index, generated per member
    constexpr T& operator[](std::size_t i) {
        template for (constexpr std::size_t k : std::define_static_array(std::views::iota(0uz, size)))
            if (i == k) return this->[:members[k]:];
        std::unreachable();
    }

    // swizzle by letters: v.sw<"zyx">() or v.sw<"xy">()
    // the result uses the first S.size letters of this vector's names, like GLSL
    template <std::size_t K>
    static consteval names<K + 1> prefix() {
        char buf[K + 1]{};
        for (std::size_t i = 0; i < K; ++i) buf[i] = Ns.s[i];
        return names<K + 1>(buf);
    }

    template <names S>
    constexpr auto sw() const {
        using out_t = vec<T, prefix<S.size>()>;
        out_t out{};
        template for (constexpr std::size_t k : std::define_static_array(std::views::iota(0uz, S.size))) {
            constexpr std::size_t src = std::string_view(Ns.s).find(S.s[k]);
            static_assert(src != std::string_view::npos, "unknown swizzle letter");
            out.[:out_t::members[k]:] = this->[:members[src]:];
        }
        return out;
    }

    friend constexpr vec operator+(vec a, const vec& b) {
        template for (constexpr auto m : members) a.[:m:] += b.[:m:];
        return a;
    }
    friend constexpr T dot(const vec& a, const vec& b) {
        T t{};
        template for (constexpr auto m : members) t += a.[:m:] * b.[:m:];
        return t;
    }
};

// ---- user code
using vec3 = vec<float, "xyz">;
using rgba = vec<unsigned char, "rgba">;

int main() {
    vec3 v{1, 2, 3};
    v.y = 20;                       // a real member, no function call
    vec3 w = v + vec3{10, 10, 10};
    std::println("v = {} {} {}, w.z = {}, v[1] = {}", v.x, v.y, v.z, w.z, v[1]);
    std::println("dot = {}", dot(v, w));
    auto zyx = v.sw<"zyx">();       // GLSL style swizzle
    auto xy = v.sw<"xy">();         // narrower vector with members x, y
    std::println("zyx = {} {} {}, xy = {} {}", zyx.x, zyx.y, zyx.z, xy.x, xy.y);
    rgba c{255, 128, 0, 255};
    std::println("color r={} a={} size={}", +c.r, +c.a, rgba::size);
    static_assert(sizeof(vec3) == 3 * sizeof(float));
    static_assert(std::is_aggregate_v<vec3>);
}

// output:
// v = 1 20 3, w.z = 13, v[1] = 20
// dot = 650
// zyx = 3 20 1, xy = 1 20
// color r=255 a=255 size=4
