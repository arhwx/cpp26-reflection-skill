#include "solution.hpp"

#include <array>
#include <cstddef>
#include <meta>
#include <print>
#include <string_view>

// counter tags, never defined
struct Requests;
struct Errors;
struct Unused;

constexpr std::size_t requests_at_start = ct::count(^^Requests);
consteval { ct::bump(^^Requests); }
constexpr std::size_t requests_after_one = ct::count(^^Requests);
consteval {
    ct::bump(^^Requests);
    ct::bump(^^Requests);
    ct::bump(^^Errors);
}
std::array<int, ct::count(^^Requests)> buffer{};
static_assert(ct::count(^^Requests) == 3 && ct::count(^^Errors) == 1);

// list tags
struct Plugins;
struct Codecs;

struct Gzip {
    static constexpr std::string_view name = "gzip";
    int level = 6;
};
consteval { ct::add(^^Plugins, ^^Gzip); }
constexpr std::size_t plugins_early = ct::types(^^Plugins).size();

// evaluated here, so it sees only what was added above this function
void print_early_plugins() {
    template for (constexpr std::meta::info p : std::define_static_array(ct::types(^^Plugins))) {
        using Plugin = typename [:p:];
        std::println("  early: {}", Plugin::name);
    }
}

struct Zstd {
    static constexpr std::string_view name = "zstd";
    int level = 19;
};
struct Brotli {
    static constexpr std::string_view name = "brotli";
    int level = 11;
};
struct Lz4 {
    static constexpr std::string_view name = "lz4";
    int level = 1;
};

consteval {
    ct::add(^^Plugins, ^^Brotli);
    ct::add(^^Codecs, ^^Lz4);
    ct::add(^^Plugins, ^^Zstd);
    ct::add(^^Codecs, ^^Zstd);
    ct::add(^^Plugins, ^^Gzip);
}
consteval { ct::bump(^^Plugins); }
constexpr int lz4_before = ct::index_of(^^Plugins, ^^Lz4);
consteval { ct::add(^^Plugins, ^^Lz4); }
consteval { ct::bump(^^Requests); }

template <std::meta::info Tag>
void print_list(std::string_view label) {
    std::print("{}:", label);
    template for (constexpr std::meta::info p : std::define_static_array(ct::types(Tag))) {
        using Plugin = typename [:p:];
        std::print(" {}={}", Plugin::name, Plugin{}.level);
    }
    std::println();
}

int main() {
    std::println("requests at start: {}", requests_at_start);
    std::println("requests after one bump: {}", requests_after_one);
    std::println("buffer size: {}", buffer.size());
    std::println("requests now: {}", ct::count(^^Requests));
    std::println("errors now: {}", ct::count(^^Errors));
    std::println("unused: {}", ct::count(^^Unused));

    std::println("plugins after the first add: {}", plugins_early);
    print_early_plugins();
    constexpr std::size_t plugins_now = ct::types(^^Plugins).size();
    std::println("plugins now: {}", plugins_now);
    print_list<^^Plugins>("plugins");
    print_list<^^Codecs>("codecs");
    constexpr std::size_t unused_types = ct::types(^^Unused).size();
    std::println("unused types: {}", unused_types);

    std::println("bumps of Plugins: {}", ct::count(^^Plugins));
    std::println("bumps of Codecs: {}", ct::count(^^Codecs));
    std::println("Zstd in plugins: {}", ct::index_of(^^Plugins, ^^Zstd));
    std::println("Zstd in codecs: {}", ct::index_of(^^Codecs, ^^Zstd));
    std::println("Gzip in codecs: {}", ct::index_of(^^Codecs, ^^Gzip));
    std::println("Lz4 in plugins before it was added: {}", lz4_before);
    std::println("Lz4 in plugins now: {}", ct::index_of(^^Plugins, ^^Lz4));
}
