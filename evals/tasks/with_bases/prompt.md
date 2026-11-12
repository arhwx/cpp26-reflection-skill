I need a debug `to_string` for structs that use inheritance, showing inherited members too.

API: `template <class T> std::string to_string(const T& value)`.

Format:
- `{name=value, name=value}` with members separated by `, `. All members are `int`.
- Base class members come first: for each base, in the order the bases are listed, its own bases first (recursively), then its members. `T`'s own members come last.
- Bases are public and non-virtual. Empty bases add nothing, and a type with no members at all prints `{}`.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>

struct A {
    int a = 1;
};

struct B : A {
    int b = 2;
};

struct C : B {
    int c = 3;
};

struct X {
    int x = 10;
};

struct Y {
    int y = 20;
};

struct M : X, Y {
    int m = 30;
};

struct Empty {};

struct E : Empty {
    int e = 5;
};

struct N : C, M {
    int n = 0;
};

struct Tag : Empty {};

int main() {
    std::println("{}", to_string(A{}));
    std::println("{}", to_string(C{}));
    std::println("{}", to_string(M{}));
    std::println("{}", to_string(E{}));

    N n;
    n.a = 7;
    n.n = 9;
    std::println("{}", to_string(n));
    std::println("{}", to_string(Tag{}));
}
```

It must print exactly:

```
{a=1}
{a=1, b=2, c=3}
{x=10, y=20, m=30}
{e=5}
{a=7, b=2, c=3, x=10, y=20, m=30, n=9}
{}
```

Write the complete header to `solution.hpp` in the current directory.
