Our command help screen lists the functions we expose, and I want each one shown in a short form: the function name followed by its parameter names, taken straight from the declaration.

API:
- `template <std::meta::info F> std::string signature()`: `F` is a reflection of a function (the test passes things like `^^add`). Returns the function name, then the parameter names in parentheses separated by `, `, for example `add(lhs, rhs)`. A function without parameters gives `reset()`.
- A parameter that has no name is written as `_`.

It must work for free functions, functions in namespaces, and member functions (static or not). Only the plain function name is printed, without namespace or class prefix.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>

int add(int lhs, int rhs);
void reset();
double scale(double value, int);
void log_line(const char* text, int level = 0);

namespace geo {
float distance(float x1, float y1, float x2, float y2);
}

struct Shape {
    void move(int dx, int dy);
    static Shape unit(double size);
};

int main() {
    std::println("{}", signature<^^add>());
    std::println("{}", signature<^^reset>());
    std::println("{}", signature<^^scale>());
    std::println("{}", signature<^^log_line>());
    std::println("{}", signature<^^geo::distance>());
    std::println("{}", signature<^^Shape::move>());
    std::println("{}", signature<^^Shape::unit>());
}
```

It must print exactly:

```
add(lhs, rhs)
reset()
scale(value, _)
log_line(text, level)
distance(x1, y1, x2, y2)
move(dx, dy)
unit(size)
```

Write the complete header to `solution.hpp` in the current directory.
