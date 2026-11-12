When a test compares two big config objects and fails, I want to see exactly which values differ. I need a generic diff for our structs that lists every changed value with its path.

API: `template <class T> std::vector<std::string> diff(const T& a, const T& b)`: one line for each leaf value that differs between `a` and `b`, as `path: old -> new`, where `old` is the value in `a` and `new` the value in `b`. Equal objects give an empty vector.

Rules:
- The non-static data members of `T` are compared in declaration order. Static data members are ignored. All members are public.
- A member of struct type is compared member by member, recursively. A C array is compared element by element, recursively: arrays can be multi-dimensional and can hold structs.
- Leaves are `bool`, integer and floating-point types, and `std::string`.
- The path joins member names with `.` and adds `[i]` for array elements, for example `origin.y`, `corners[1].x`, `grid[1][0]`.
- Numbers and `bool` are formatted the way `std::format("{}", v)` does it. Strings are written in double quotes, no escaping needed.
- Lines come in the order the leaves are declared, depth first.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>
#include <vector>

struct Point {
    int x;
    int y;
};

struct Pen {
    std::string color;
    double width;
};

struct Style {
    Pen pen;
    bool filled;
    float opacity;
};

struct Shape {
    const std::string id;
    Point origin;
    Point corners[3];
    int grid[2][2];
    std::string tags[2];
    Style style;
    static inline int created = 0;
    long version;
};

struct Team {
    std::string name;
    int scores[3];
};

struct League {
    Team teams[2];
    unsigned round;
};

void show(const std::vector<std::string>& lines) {
    if (lines.empty())
        std::println("no differences");
    for (const std::string& line : lines)
        std::println("{}", line);
    std::println("--");
}

int main() {
    const Shape a{"s1", {0, 0}, {{1, 2}, {3, 4}, {5, 6}}, {{1, 2}, {3, 4}}, {"red", "big"}, {{"black", 1.5}, true, 0.5f}, 1};
    show(diff(a, a));

    Shape b = a;
    b.origin.y = 5;
    b.corners[1].x = 30;
    b.corners[2].y = -6;
    b.grid[1][0] = 9;
    b.tags[1] = "small";
    b.style.pen.color = "blue";
    b.style.filled = false;
    b.style.opacity = 0.25f;
    b.version = 2;
    show(diff(a, b));

    const Shape c{"s2", {0, 0}, {{1, 2}, {3, 4}, {5, 6}}, {{1, 2}, {3, 4}}, {"red", "big"}, {{"black", 2.0}, true, 0.5f}, 1};
    show(diff(a, c));

    League before{{{"ants", {1, 2, 3}}, {"bees", {4, 5, 6}}}, 1};
    League after = before;
    after.teams[1].scores[2] = 7;
    after.teams[0].name = "wasps";
    after.round = 2;
    show(diff(before, after));
    show(diff(after, before));

    show(diff(Point{1, 2}, Point{-1, 2}));
}
```

It must print exactly:

```
no differences
--
origin.y: 0 -> 5
corners[1].x: 3 -> 30
corners[2].y: 6 -> -6
grid[1][0]: 3 -> 9
tags[1]: "big" -> "small"
style.pen.color: "black" -> "blue"
style.filled: true -> false
style.opacity: 0.5 -> 0.25
version: 1 -> 2
--
id: "s1" -> "s2"
style.pen.width: 1.5 -> 2
--
teams[0].name: "ants" -> "wasps"
teams[1].scores[2]: 6 -> 7
round: 1 -> 2
--
teams[0].name: "wasps" -> "ants"
teams[1].scores[2]: 7 -> 6
round: 2 -> 1
--
x: 1 -> -1
--
```

Write the complete header to `solution.hpp` in the current directory.
