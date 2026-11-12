I need a small type-level helper. Normally I'd write it with recursive templates, but I'd like to see how it looks with C++26 reflection.

API:
- `template <class Tuple> using filter_integral_t = ...;` where `Tuple` is a `std::tuple<Ts...>`. It names the `std::tuple` of those `Ts` for which `std::is_integral_v` is true, in the original order. Duplicates are kept, and no integral types gives `std::tuple<>`.

Everything is checked with `static_assert`s in the test; the program only prints `OK`.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <cstddef>
#include <print>
#include <string>
#include <tuple>
#include <type_traits>

static_assert(std::is_same_v<filter_integral_t<std::tuple<int, double, long, char, float>>, std::tuple<int, long, char>>);
static_assert(std::is_same_v<filter_integral_t<std::tuple<double, float, std::string>>, std::tuple<>>);
static_assert(std::is_same_v<filter_integral_t<std::tuple<>>, std::tuple<>>);
static_assert(std::is_same_v<filter_integral_t<std::tuple<bool, int&, const short, int*, std::size_t>>,
                             std::tuple<bool, const short, std::size_t>>);
static_assert(std::is_same_v<filter_integral_t<std::tuple<int, int, unsigned char>>, std::tuple<int, int, unsigned char>>);

template <class... Ts>
constexpr std::size_t count_integral() {
    return std::tuple_size_v<filter_integral_t<std::tuple<Ts...>>>;
}

static_assert(count_integral<char, double, long long, void*>() == 2);

int main() {
    std::println("OK");
}
```

It must print exactly:

```
OK
```

Write the complete header to `solution.hpp` in the current directory.
