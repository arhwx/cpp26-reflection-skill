We have a few bit-flag enums and turn them into strings with X-macros. This is the current header:

```cpp
#pragma once

#include <string>

#define PERM_FLAGS(X) \
    X(read, 1u << 0)  \
    X(write, 1u << 1) \
    X(exec, 1u << 2)  \
    X(admin, 1u << 3)

enum class Perm : unsigned {
    none = 0,
#define X(name, value) name = value,
    PERM_FLAGS(X)
#undef X
    all = 0xFu,
};

inline std::string flags_to_string(Perm p) {
    std::string out;
#define X(name, value)                         \
    if (static_cast<unsigned>(p) & (value)) { \
        if (!out.empty())                      \
            out += '|';                        \
        out += #name;                          \
    }
    PERM_FLAGS(X)
#undef X
    return out.empty() ? "none" : out;
}
```

I want to get rid of the macros. From now on the enums are declared as normal enums where they are used (see `Perm` and `Opt` in the test), and the header should only provide `flags_to_string`, built with reflection so it works for any of our flag enums.

API:
- `flags_to_string(e)` for a value `e` of any enum type, returning `std::string`.

The output must stay exactly what the macro version produces for `Perm`: the names of the set flags joined with `|` in declaration order, `none` when no flag is set, and bits that don't belong to any flag are ignored. Other flag enums follow the same rules.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <cstdint>
#include <print>

enum class Perm : unsigned {
    none = 0,
    read = 1u << 0,
    write = 1u << 1,
    exec = 1u << 2,
    admin = 1u << 3,
    all = 0xFu,
};

enum class Opt : std::uint8_t { fast = 1, small = 2, debug = 16 };

int main() {
    for (unsigned v : {0u, 1u, 3u, 5u, 15u, 8u, 0x21u})
        std::println("{} -> {}", v, flags_to_string(static_cast<Perm>(v)));
    std::println("{}", flags_to_string(Perm::all));
    std::println("{}", flags_to_string(static_cast<Opt>(17)));
    std::println("{}", flags_to_string(Opt{}));
}
```

It must print exactly:

```
0 -> none
1 -> read
3 -> read|write
5 -> read|exec
15 -> read|write|exec|admin
8 -> admin
33 -> read
read|write|exec|admin
fast|debug
none
```

Write the complete header to `solution.hpp` in the current directory.
