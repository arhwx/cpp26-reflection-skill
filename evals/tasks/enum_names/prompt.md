I keep writing switch statements to turn enums into strings and back. I'd like a header that does it for any enum, without listing the enumerators by hand.

API:
- `template <class E> constexpr std::string_view enum_name(E value)`: the name of the enumerator with that value, or an empty `string_view` if the value is not one of the enumerators.
- `template <class E> constexpr std::optional<E> enum_from_name(std::string_view name)`: the enumerator with exactly that name (case-sensitive), or `std::nullopt`.

Names are the plain enumerator names, without the enum or namespace prefix. Both functions must work for scoped and unscoped enums with any underlying type, and must be usable in constant expressions (the test has `static_assert`s).

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <optional>
#include <print>
#include <string_view>

enum class Color { red, green, blue };
enum class Status : short { ok = 0, warn = 2, error = 5, fatal = -1 };
enum Legacy { first = 1, second = 4 };

namespace net {
enum class Proto : unsigned char { tcp = 6, udp = 17 };
}

static_assert(enum_name(Color::blue) == "blue");
static_assert(enum_from_name<Status>("fatal") == Status::fatal);

int main() {
    Status values[] = {Status::ok, Status::warn, static_cast<Status>(3), Status::error, Status::fatal};
    for (Status s : values)
        std::println("{} -> [{}]", static_cast<int>(s), enum_name(s));

    std::println("{} {} {}", enum_name(Color::green), enum_name(second), enum_name(net::Proto::udp));
    std::println("[{}]", enum_name(static_cast<Legacy>(2)));

    std::string_view names[] = {"warn", "fatal", "ok", "Warn", "unknown", ""};
    for (std::string_view n : names) {
        std::optional<Status> s = enum_from_name<Status>(n);
        if (s)
            std::println("[{}] -> {}", n, static_cast<int>(*s));
        else
            std::println("[{}] -> nullopt", n);
    }

    std::optional<net::Proto> p = enum_from_name<net::Proto>("tcp");
    std::println("tcp -> {}", p ? static_cast<int>(*p) : -1);
}
```

It must print exactly:

```
0 -> [ok]
2 -> [warn]
3 -> []
5 -> [error]
-1 -> [fatal]
green second udp
[]
[warn] -> 2
[fatal] -> -1
[ok] -> 0
[Warn] -> nullopt
[unknown] -> nullopt
[] -> nullopt
tcp -> 6
```

Write the complete header to `solution.hpp` in the current directory.
