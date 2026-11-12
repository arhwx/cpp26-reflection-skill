I'm writing some compile-time checks for our classes and need two helpers.

API:
- `template <class T> constexpr std::size_t field_count()` (`consteval` is fine too): the number of non-static data members of `T`, whatever their access. Public, protected and private members all count; static members don't. It must be usable in `static_assert`.
- `template <class T> std::vector<std::string_view> public_field_names()`: the names of the public non-static data members of `T` only, in declaration order.

The classes can't be modified for this (no friend declarations or macros).

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>
#include <string_view>

class Widget {
public:
    Widget() = default;
    int area() const { return w_ * h_; }

    int id = 0;
    std::string label;

protected:
    double scale_ = 1.0;

private:
    int w_ = 0;
    int h_ = 0;
    static inline int count_ = 0;

public:
    bool visible = true;
};

struct Plain {
    int a;
    int b;
};

class Hidden {
    int secret_ = 0;
};

static_assert(field_count<Widget>() == 6);
static_assert(field_count<Plain>() == 2);
static_assert(field_count<Hidden>() == 1);

int main() {
    std::println("{}", field_count<Widget>());
    for (std::string_view n : public_field_names<Widget>())
        std::println("{}", n);
    std::println("{} {}", public_field_names<Plain>().size(), public_field_names<Hidden>().size());
}
```

It must print exactly:

```
6
id
label
visible
2 0
```

Write the complete header to `solution.hpp` in the current directory.
