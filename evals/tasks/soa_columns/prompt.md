For some hot loops we want a struct-of-arrays layout, generated from the plain struct instead of written by hand.

API:
- `soa_columns<T>`: a struct type with one public data member for each non-static data member of `T`, with the same name and in the same order, whose type is `std::vector` of that member's type. For `struct Point { int x; double y; };`, `soa_columns<Point>` has `std::vector<int> x;` and `std::vector<double> y;`.
- `template <class T> void push(soa_columns<T>& columns, const T& value)`: appends each member of `value` to its column.

Nothing should have to be written per struct type.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>
#include <type_traits>
#include <vector>

struct Point {
    int x;
    double y;
};

struct Particle {
    float mass;
    std::string label;
    int id;
};

static_assert(std::is_same_v<decltype(soa_columns<Point>::x), std::vector<int>>);
static_assert(std::is_same_v<decltype(soa_columns<Point>::y), std::vector<double>>);
static_assert(std::is_same_v<decltype(soa_columns<Particle>::label), std::vector<std::string>>);

int main() {
    soa_columns<Point> c;
    c.x.push_back(1);
    c.y.push_back(2.5);
    push(c, Point{3, 4.5});
    push(c, Point{5, 6.5});
    std::println("x: {}", c.x);
    std::println("y: {}", c.y);

    soa_columns<Particle> p;
    push(p, Particle{1.5f, "a", 7});
    push(p, Particle{0.25f, "b", 8});
    std::println("mass: {}", p.mass);
    std::println("label: {}", p.label);
    std::println("id: {}", p.id);
}
```

It must print exactly:

```
x: [1, 3, 5]
y: [2.5, 4.5, 6.5]
mass: [1.5, 0.25]
label: ["a", "b"]
id: [7, 8]
```

Write the complete header to `solution.hpp` in the current directory.
