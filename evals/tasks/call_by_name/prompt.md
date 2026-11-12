For a debug console I want to call getters on an object, picking the function by a name typed at run time.

API:
- `template <class T> std::optional<int> call_by_name(T& obj, std::string_view name)`: if `T` has a public non-static member function called `name` that takes no arguments and returns `int`, call it on `obj` and return the result. Otherwise return `std::nullopt` and call nothing.

Names are matched exactly (case-sensitive). It has to work for any class, without registering the functions by hand.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <string_view>

class Counter {
public:
    Counter() = default;
    explicit Counter(int start) : value(start) {}
    Counter(const Counter&) = default;
    Counter& operator=(const Counter&) = default;
    ~Counter() {}

    bool operator==(const Counter&) const = default;
    Counter& operator++() {
        ++value;
        return *this;
    }
    int operator()() const { return -1; }

    int get() const { return value; }
    int doubled() const { return value * 2; }
    int bump() { return ++value; }
    int total() const { return value + step; }
    int total(int extra) const { return value + step + extra; }
    int add(int n) const { return value + n; }
    static int zero() { return 0; }
    void reset() { value = 0; }

    int value = 0;
    int step = 10;
    std::string label = "counter";

private:
    int secret() const { return 42; }
};

void show(Counter& c, std::string_view name) {
    std::optional<int> r = call_by_name(c, name);
    if (r)
        std::println("[{}] -> {}", name, *r);
    else
        std::println("[{}] -> nullopt", name);
}

int main() {
    Counter c(3);
    const char* names[] = {"get",   "doubled", "bump",    "bump",     "get",        "total",
                           "add",   "secret",  "zero",    "value",    "label",      "Counter",
                           "~Counter", "operator()", "Get", "",        "missing",    "reset"};
    for (const char* n : names)
        show(c, n);
}
```

It must print exactly:

```
[get] -> 3
[doubled] -> 6
[bump] -> 4
[bump] -> 5
[get] -> 5
[total] -> 15
[add] -> nullopt
[secret] -> nullopt
[zero] -> nullopt
[value] -> nullopt
[label] -> nullopt
[Counter] -> nullopt
[~Counter] -> nullopt
[operator()] -> nullopt
[Get] -> nullopt
[] -> nullopt
[missing] -> nullopt
[reset] -> nullopt
```

Write the complete header to `solution.hpp` in the current directory.
