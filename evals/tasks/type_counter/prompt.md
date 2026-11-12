I want compile-time state in standard C++26: counters that later code in the same file can read, and lists of types that a plugin can join at namespace scope right after its definition, so that `main` can loop over every registered plugin with no central list kept by hand. It must be deterministic and well-formed standard C++, with no macros, no `__COUNTER__` and no friend injection tricks.

Everything goes in `namespace ct`. A tag is a reflection of a class type, which may be incomplete. Each tag has its own counter and its own list of types, independent of every other tag, and a tag's counter and list are independent of each other.
- `consteval void ct::bump(std::meta::info tag)`: adds 1 to the counter of `tag`.
- `consteval std::size_t ct::count(std::meta::info tag)`: the counter of `tag`. It starts at 0.
- `consteval void ct::add(std::meta::info tag, std::meta::info type)`: appends `type` to the list of `tag`. If `type` is already in that list, it does nothing.
- `consteval std::vector<std::meta::info> ct::types(std::meta::info tag)`: the list of `tag`, in the order the types were first added. It starts empty.
- `consteval int ct::index_of(std::meta::info tag, std::meta::info type)`: the position of `type` in the list of `tag`, counting from 0, or `-1` if it is not there.

When state changes and what a query sees:
- `bump` and `add` are called from `consteval { }` blocks at namespace scope, as in the test, possibly several times in one block.
- A query (`count`, `types`, `index_of`) sees exactly the calls to `bump` and `add` in the blocks that come before it in the file, plus the earlier calls in the same constant evaluation. So in one block, a call sees the calls before it in that block.
- A query in the body of a non-template function sees the state at the place where that function is defined, even when the function is called later. Queries in `main`, and in the function template `print_list` that only `main` uses, see every call the test makes.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
requests at start: 0
requests after one bump: 1
buffer size: 3
requests now: 4
errors now: 1
unused: 0
plugins after the first add: 1
  early: gzip
plugins now: 4
plugins: gzip=6 brotli=11 zstd=19 lz4=1
codecs: lz4=1 zstd=19
unused types: 0
bumps of Plugins: 1
bumps of Codecs: 0
Zstd in plugins: 2
Zstd in codecs: 1
Gzip in codecs: -1
Lz4 in plugins before it was added: -1
Lz4 in plugins now: 3
```

Write the complete header to `solution.hpp` in the current directory.
