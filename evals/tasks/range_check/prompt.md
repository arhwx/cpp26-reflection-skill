I'd like to declare value ranges right on struct members and check objects against them, like this: `[[=check::range{0, 150}]] int age;`.

API:
- `namespace check { struct range { int min; int max; }; }`: the annotation type. Both bounds are inclusive.
- `template <class T> std::vector<std::string> validate(const T& obj)`: one message for every member with a `check::range` annotation whose value is outside the range, in declaration order. The message is the member name followed by ` out of range`, for example `age out of range`. An empty vector means the object is valid.

Annotated members are always `int`. Members without a range annotation are not checked. The annotation must compile exactly as written in the test.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>
#include <vector>

struct Player {
    std::string name;
    [[=check::range{0, 150}]] int age;
    [[=check::range{1, 10}]] int level;
    int score;
    [[=check::range{-50, 50}]] int balance;
};

struct Plain {
    int x;
    int y;
};

void report(const std::vector<std::string>& errors) {
    if (errors.empty()) {
        std::println("valid");
        return;
    }
    for (const std::string& e : errors)
        std::println("{}", e);
}

int main() {
    report(validate(Player{"ann", 30, 5, -999, 0}));
    std::println("--");
    report(validate(Player{"bob", 151, 0, 7, 50}));
    std::println("--");
    report(validate(Player{"cid", -1, 10, 0, -51}));
    std::println("--");
    report(validate(Player{"dee", 0, 1, 0, -50}));
    std::println("--");
    report(validate(Plain{-1000, 1000}));
}
```

It must print exactly:

```
valid
--
age out of range
level out of range
--
age out of range
balance out of range
--
valid
--
valid
```

Write the complete header to `solution.hpp` in the current directory.
