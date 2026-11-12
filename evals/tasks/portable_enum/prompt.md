Every time someone adds a color, they forget to update the switch below. This is our current `color.hpp`:

```cpp
#pragma once

#include <string_view>

enum class Color : unsigned char { black, red = 4, green, blue, white = 15 };

constexpr std::string_view color_name(Color c) {
    switch (c) {
    case Color::black: return "black";
    case Color::red: return "red";
    case Color::green: return "green";
    case Color::blue: return "blue";
    case Color::white: return "white";
    }
    return "unknown";
}
```

I'd like `color_name` to get the names from C++26 static reflection when the compiler supports it. The catch: the same header is also used by projects that are still on plain C++20, and there it must compile and give exactly the same results.

Keep the enum and the function as they are: `constexpr std::string_view color_name(Color c)`, returning the enumerator name, or `unknown` for a value that has no name. It must stay usable in constant expressions (the test has `static_assert`s), and the test program itself is plain C++20.

We build with GCC 16 (`g++ -std=c++26 -freflection`) and with plain C++20 (`g++ -std=c++20`, no reflection flag). It should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <iostream>

static_assert(color_name(Color::green) == "green");
static_assert(color_name(static_cast<Color>(3)) == "unknown");

int main() {
    const Color colors[] = {Color::black, Color::red, Color::green, Color::blue, Color::white};
    for (Color c : colors)
        std::cout << static_cast<int>(c) << " -> " << color_name(c) << '\n';
    std::cout << "3 -> " << color_name(static_cast<Color>(3)) << '\n';
    std::cout << "255 -> " << color_name(static_cast<Color>(255)) << '\n';
}
```

It must print exactly:

```
0 -> black
4 -> red
5 -> green
6 -> blue
15 -> white
3 -> unknown
255 -> unknown
```

Write the complete header to `solution.hpp` in the current directory.
