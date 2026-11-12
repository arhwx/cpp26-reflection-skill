Our generic code often needs the same nested type with one element type swapped, for example every `int` in `std::map<int, std::vector<int>>` turned into `std::string`. Today we write a separate trait for each template shape, and shapes with value parameters such as `std::array<T, N>` need their own specializations. I want one alias that works for every shape.

API:
- `template <class T, class From, class To> using replace_t = ...;`: the type `T` with every occurrence of the type `From` replaced by `To`.

Rules, checked in this order:
- If `T` is `From`, the result is `To`. `To` is used as it is; it is not searched again.
- If `T` is a const-qualified type `const U`, the result is `const R`, where `R` is the result for `U`.
- If `T` is a pointer type `U*`, the result is `R*`, where `R` is the result for `U`.
- If `T` is a specialization of a class template, the result is the same template specialized with the replaced arguments. Type arguments are replaced by these same rules, at any depth. All other template arguments, such as values and templates, are kept as they are. Default arguments count too, so `std::vector<int>` becomes `std::vector<long>`.
- Any other type is kept as it is. Members of a class are not looked into: `Plain` in the test stays `Plain`.

A type alias means the type it names, wherever it appears. Volatile, reference, array and function types will not be asked for.

Everything is checked with `static_assert`s in the test, and the program prints a few values stored in types built with `replace_t`.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <array>
#include <map>
#include <print>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

template <class T>
struct Box {
    T value;
};

template <class A, class B>
struct Pair {
    A first;
    B second;
};

template <class T, int N>
struct Fixed {
    T data[N];
};

template <template <class> class C, class T>
struct Holder {
    C<T> inner;
};

template <class... Ts>
struct List {};

struct Plain {
    int x;
};

using Int = int;
template <class T>
using Ptr = T*;

// aliases
static_assert(std::is_same_v<replace_t<Box<Int>, int, long>, Box<long>>);
static_assert(std::is_same_v<replace_t<Box<int>, Int, long>, Box<long>>);
static_assert(std::is_same_v<replace_t<Ptr<Int>, int, char>, char*>);

// T itself
static_assert(std::is_same_v<replace_t<int, int, long>, long>);
static_assert(std::is_same_v<replace_t<double, int, long>, double>);
static_assert(std::is_same_v<replace_t<Plain, int, long>, Plain>);

// template arguments at any depth
static_assert(std::is_same_v<replace_t<Pair<int, Box<int>>, int, long>, Pair<long, Box<long>>>);
static_assert(std::is_same_v<replace_t<Box<Box<Box<char>>>, char, bool>, Box<Box<Box<bool>>>>);
static_assert(std::is_same_v<replace_t<Pair<Plain, double>, int, long>, Pair<Plain, double>>);
static_assert(std::is_same_v<replace_t<List<int, char, int>, int, double>, List<double, char, double>>);
static_assert(std::is_same_v<replace_t<List<>, int, long>, List<>>);

// other template arguments are kept
static_assert(std::is_same_v<replace_t<Fixed<int, 3>, int, long>, Fixed<long, 3>>);
static_assert(std::is_same_v<replace_t<Holder<Box, int>, int, long>, Holder<Box, long>>);
static_assert(std::is_same_v<replace_t<std::array<Box<int>, 4>, int, char>, std::array<Box<char>, 4>>);

// const and pointers
static_assert(std::is_same_v<replace_t<std::tuple<int, const int*>, int, long>, std::tuple<long, const long*>>);
static_assert(std::is_same_v<replace_t<const Box<int>* const, int, long>, const Box<long>* const>);
static_assert(std::is_same_v<replace_t<int**, int, char>, char**>);
static_assert(std::is_same_v<replace_t<Box<const int>, int, Plain>, Box<const Plain>>);
static_assert(std::is_same_v<replace_t<const int*, const int, char>, char*>);

// From and To can be any type
static_assert(std::is_same_v<replace_t<Pair<Box<int>, int>, Box<int>, char>, Pair<char, int>>);
static_assert(std::is_same_v<replace_t<Pair<int, char>, int, Box<int>>, Pair<Box<int>, char>>);
static_assert(std::is_same_v<replace_t<Box<int*>, int*, const char*>, Box<const char*>>);

// standard templates, including their default arguments
static_assert(std::is_same_v<replace_t<std::string, char, wchar_t>, std::wstring>);
static_assert(std::is_same_v<replace_t<std::vector<int>, int, long>, std::vector<long>>);
static_assert(std::is_same_v<replace_t<std::map<int, Box<int>>, int, long>, std::map<long, Box<long>>>);

int main() {
    replace_t<std::map<int, std::vector<int>>, int, std::string> m;
    m["a"].push_back("x");
    m["a"].push_back("y");
    m["b"] = {"z"};
    for (const auto& [key, values] : m)
        std::println("{} {}", key, values);

    replace_t<Pair<int, Fixed<int, 2>>, int, double> p{0.5, {{1.25, 2.5}}};
    std::println("{} {} {}", p.first, p.second.data[0], p.second.data[1]);
}
```

It must print exactly:

```
a ["x", "y"]
b ["z"]
0.5 1.25 2.5
```

Write the complete header to `solution.hpp` in the current directory.
