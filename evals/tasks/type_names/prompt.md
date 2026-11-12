We write type names into log lines and into the type tags of a serialized format. We use `typeid(T).name()` today, but its output differs between compilers and standard libraries, so the tags don't match across platforms. I want a name that depends only on the source code.

API:
- `template <class T> std::string type_name()`: the name of `T` by the rules below.

Rules:
- These fundamental types are named as written here: `bool`, `char`, `int`, `unsigned` (for `unsigned int`), `long`, `double`. No other fundamental types are used.
- A class or enumeration type that is not a template specialization is named by its own unqualified name: `geo::Vec` gives `Vec`, and `Outer::Inner` gives `Inner`.
- A class template specialization is named `Name<arg1, arg2>`: the unqualified template name, then its template arguments in angle brackets separated by `, `. Type arguments are named by these same rules, and `int` arguments are written in decimal. A specialization with no arguments gives `Name<>`.
- A type alias is named like the type it stands for: `IntBox` gives `Box<int>`.
- cv-qualified, reference, pointer and array types, and non-type arguments other than `int`, will not be asked for.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>

enum class Color { red, green };

struct Point {
    int x;
    int y;
};

struct Outer {
    struct Inner {};
};

template <class T>
struct Box {
    T value;
};

template <class A, class B>
struct Pair {
    A first;
    B second;
};

template <class... Ts>
struct Tuple {};

template <int N>
struct Tag {};

template <class T, int N>
struct Fixed {
    T data[N];
};

namespace geo {
struct Vec {};
template <class T>
struct Grid {};
}  // namespace geo

using IntBox = Box<int>;
using Count = unsigned;
using Coord = Pair<long, geo::Vec>;
template <class T>
using BoxOf = Box<T>;

template <class T>
void show() {
    std::string name = type_name<T>();
    std::println("{}", name);
}

int main() {
    show<int>();
    show<unsigned>();
    show<long>();
    show<bool>();
    show<char>();
    show<double>();
    show<Point>();
    show<Color>();
    show<Outer::Inner>();
    show<geo::Vec>();
    show<Box<Point>>();
    show<Pair<int, Box<double>>>();
    show<IntBox>();
    show<Count>();
    show<Box<Count>>();
    show<BoxOf<char>>();
    show<Coord>();
    show<Pair<Coord, Count>>();
    show<geo::Grid<IntBox>>();
    show<Tuple<>>();
    show<Tuple<char, Color, Tuple<bool>>>();
    show<Tag<-3>>();
    show<Fixed<Box<long>, 4>>();
    show<Box<Fixed<Outer::Inner, 2>>>();
}
```

It must print exactly:

```
int
unsigned
long
bool
char
double
Point
Color
Inner
Vec
Box<Point>
Pair<int, Box<double>>
Box<int>
unsigned
Box<unsigned>
Box<char>
Pair<long, Vec>
Pair<Pair<long, Vec>, unsigned>
Grid<Box<int>>
Tuple<>
Tuple<char, Color, Tuple<bool>>
Tag<-3>
Fixed<Box<long>, 4>
Box<Fixed<Inner, 2>>
```

Write the complete header to `solution.hpp` in the current directory.
