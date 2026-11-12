I want a generic `to_string` for our plain structs so I can log them without writing a printer for each one.

API: `template <class T> std::string to_string(const T& value)`.

Format:
- A struct prints as `{name=value, name=value}`: members in declaration order, separated by `, `. A struct with no members prints `{}`.
- Members are `int`, `double`, `std::string` or another struct, which is printed recursively the same way.
- `int` and `double` are formatted the way `std::format("{}", v)` does it (so `3.0` prints as `3`).
- `std::string` is printed in double quotes, no escaping needed.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>

struct Inner {
    int a;
};

struct Point {
    int x;
    double y;
    std::string name;
    Inner inner;
};

struct Empty {};

struct Deep {
    Point p;
    Empty e;
    double ratio;
};

int main() {
    std::println("{}", to_string(Point{1, 2.5, "ab", {3}}));
    std::println("{}", to_string(Inner{-7}));
    std::println("{}", to_string(Empty{}));
    std::println("{}", to_string(Deep{{0, 0.1, "", {42}}, {}, 3.0}));

    Point p{10, -0.5, "long name with spaces", {0}};
    p.inner.a = 99;
    std::println("{}", to_string(p));
}
```

It must print exactly:

```
{x=1, y=2.5, name="ab", inner={a=3}}
{a=-7}
{}
{p={x=0, y=0.1, name="", inner={a=42}}, e={}, ratio=3}
{x=10, y=-0.5, name="long name with spaces", inner={a=99}}
```

Write the complete header to `solution.hpp` in the current directory.
