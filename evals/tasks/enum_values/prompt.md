I keep writing `kAllLevels` arrays next to my enums so I can loop over them and map values to table rows, and they go stale whenever someone adds an enumerator. I want these generated.

API:
- `template <class E> constexpr std::size_t enum_count = ...;`: a variable template holding the number of enumerators declared in `E`.
- `template <class E> constexpr std::array<E, enum_count<E>> enum_values()`: all enumerators of `E` in declaration order.
- `template <class E> constexpr std::optional<std::size_t> enum_index(E value)`: the position of `value` in `enum_values<E>()`, or `std::nullopt` if no enumerator has that value.

Rules:
- Enumerators that share a value are still separate entries in `enum_count` and `enum_values`. `enum_index` gives the position of the first of them.
- It must work for scoped and unscoped enumerations, including one with no enumerators.
- All three must be usable in constant expressions, as the `static_assert`s in the test do, and at run time.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <print>
#include <type_traits>
#include <utility>

enum class Level { trace = -2, debug = -1, info = 0, warn = 5, warning = 5, error, fatal = 100 };
enum Color : unsigned char { red = 1, green = 2, blue = 4 };
enum class Empty {};

static_assert(enum_count<Level> == 7);
static_assert(enum_count<Color> == 3);
static_assert(enum_count<Empty> == 0);

static_assert(std::is_same_v<decltype(enum_values<Level>()), std::array<Level, 7>>);
static_assert(enum_values<Level>() == std::array{Level::trace, Level::debug, Level::info, Level::warn, Level::warning,
                                                 Level::error, Level::fatal});
static_assert(enum_values<Color>() == std::array<Color, 3>{red, green, blue});
static_assert(enum_values<Empty>().empty());

static_assert(enum_index(Level::trace) == 0);
static_assert(enum_index(Level::warning) == 3);
static_assert(enum_index(Level::error) == 5);
static_assert(enum_index(static_cast<Level>(7)) == std::nullopt);
static_assert(enum_index(blue) == 2);
static_assert(enum_index(Empty{}) == std::nullopt);

const char* labels[enum_count<Level>] = {"T", "D", "I", "W", "W2", "E", "F"};

template <class E>
void show(E value) {
    std::optional<std::size_t> i = enum_index(value);
    if (i)
        std::println("{} -> {}", static_cast<int>(std::to_underlying(value)), *i);
    else
        std::println("{} -> none", static_cast<int>(std::to_underlying(value)));
}

int main() {
    for (Level l : enum_values<Level>())
        std::print("{} ", std::to_underlying(l));
    std::println("({})", enum_count<Level>);

    std::size_t n = 0;
    for (Level l : enum_values<Level>()) {
        std::print("{} ", labels[*enum_index(l)]);
        ++n;
    }
    std::println("({})", n);

    for (Color c : enum_values<Color>())
        show(c);
    show(static_cast<Color>(3));
    show(Level::fatal);
    show(static_cast<Level>(-3));
    show(Level{6});
    std::println("empty {}", enum_values<Empty>().size());
}
```

It must print exactly:

```
-2 -1 0 5 5 6 100 (7)
T D I W W E F (7)
1 -> 0
2 -> 1
4 -> 2
3 -> none
100 -> 6
-3 -> none
6 -> 5
empty 0
```

Write the complete header to `solution.hpp` in the current directory.
