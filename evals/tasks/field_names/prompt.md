I need the names of a struct's data members at compile time, for building tables and checking schemas in `static_assert`s.

API: `template <class T> constexpr std::array<std::string_view, N> field_names()` (`consteval` is fine too), where `N` is the number of non-static data members of `T`. The array holds their names in declaration order.

Static data members, member functions and type aliases are not fields. Bit-fields are. The result must be usable in constant expressions: the test stores it in a `constexpr` variable and checks the exact type and contents with `static_assert`.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <array>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>

struct Config {
    int port;
    std::string host;
    static inline int instances = 0;
    bool verbose;
    void reset() {}
    using size_type = unsigned;
    double timeout;
};

struct Empty {};

struct Flags {
    unsigned read : 1;
    unsigned write : 1;
    unsigned char mode;
};

constexpr auto names = field_names<Config>();
static_assert(std::is_same_v<decltype(names), const std::array<std::string_view, 4>>);
static_assert(names[0] == "port" && names[1] == "host" && names[2] == "verbose" && names[3] == "timeout");
static_assert(field_names<Empty>().empty());
static_assert(field_names<Flags>().size() == 3 && field_names<Flags>()[2] == "mode");

int main() {
    for (std::string_view n : names)
        std::println("{}", n);
    for (std::string_view n : field_names<Flags>())
        std::println("{}", n);
    std::println("{}", field_names<Empty>().size());
}
```

It must print exactly:

```
port
host
verbose
timeout
read
write
mode
0
```

Write the complete header to `solution.hpp` in the current directory.
