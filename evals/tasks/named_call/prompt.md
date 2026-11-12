Our command console calls ordinary functions with arguments typed as `name=value` pairs. I want to match the pairs to the function's parameters by name, without writing a parser for each function.

API:
- `template <std::meta::info Fn> std::optional<R> call_named(const std::map<std::string, std::string>& args)`: `Fn` is a reflection of a free function (the test passes `^^add`, `^^geo::scale` and so on), and `R` is that function's return type. Each entry of `args` maps a parameter name to its argument as text. `call_named` converts each argument to the parameter's type, calls the function and returns its result.

Rules:
- Parameter types are `int`, `double`, `bool` and `std::string`, taken by value or by const reference. The functions don't return `void`.
- `int` and `double` arguments must use the whole text (`"3x"` and `""` are errors), `bool` accepts only `true` and `false`, and `std::string` takes the text as is.
- Parameters with a default argument may be left out, but only as a trailing run: when a parameter is left out, every parameter after it must be left out too. The function is then called with fewer arguments, so it uses its own defaults. A parameter without a name can't be given, so it counts as left out.
- It returns `std::nullopt` without calling the function if a key is not the name of a parameter, a parameter is left out in any other way, or an argument can't be converted.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <map>
#include <optional>
#include <print>
#include <string>

int add(int a, int b) { return a + b; }

int divide(int num, int den) { return num / den; }

std::string greet(const std::string& name, int times = 1, bool shout = false) {
    std::string text = name;
    for (int i = 1; i < times; ++i)
        text += " " + name;
    return shout ? text + "!" : text;
}

namespace geo {
double scale(double value, double factor = 2.0) { return value * factor; }
}

int counter(int start = 10, int step = 1) { return start * 100 + step; }

int answer() { return 42; }

// the second parameter is unused, kept for old callers
int legacy(int code, int = 0) { return code * 2; }

template <class R>
void show(const char* label, const std::optional<R>& result) {
    if (result)
        std::println("{}: [{}]", label, *result);
    else
        std::println("{}: nullopt", label);
}

int main() {
    show("add", call_named<^^add>({{"a", "2"}, {"b", "3"}}));
    show("divide", call_named<^^divide>({{"num", "20"}, {"den", "4"}}));
    show("divide negative", call_named<^^divide>({{"den", "-3"}, {"num", "9"}}));
    show("add missing", call_named<^^add>({{"a", "2"}}));
    show("add unknown", call_named<^^add>({{"a", "2"}, {"b", "3"}, {"c", "4"}}));
    show("add bad int", call_named<^^add>({{"a", "2"}, {"b", "3x"}}));
    show("add empty int", call_named<^^add>({{"a", ""}, {"b", "3"}}));

    show("greet", call_named<^^greet>({{"name", "ann"}}));
    show("greet times", call_named<^^greet>({{"name", "ann"}, {"times", "3"}}));
    show("greet all", call_named<^^greet>({{"name", "ann"}, {"times", "2"}, {"shout", "true"}}));
    show("greet false", call_named<^^greet>({{"name", "bo"}, {"times", "1"}, {"shout", "false"}}));
    show("greet gap", call_named<^^greet>({{"name", "ann"}, {"shout", "true"}}));
    show("greet bad bool", call_named<^^greet>({{"name", "ann"}, {"times", "2"}, {"shout", "1"}}));
    show("greet no name", call_named<^^greet>({{"times", "2"}}));
    show("greet empty name", call_named<^^greet>({{"name", ""}}));

    show("scale", call_named<^^geo::scale>({{"value", "2.5"}}));
    show("scale factor", call_named<^^geo::scale>({{"factor", "3"}, {"value", "1.5"}}));
    show("scale bad", call_named<^^geo::scale>({{"value", "abc"}}));

    show("counter", call_named<^^counter>({}));
    show("counter start", call_named<^^counter>({{"start", "7"}}));
    show("counter step", call_named<^^counter>({{"step", "5"}}));
    show("counter both", call_named<^^counter>({{"step", "5"}, {"start", "7"}}));

    show("answer", call_named<^^answer>({}));
    show("answer extra", call_named<^^answer>({{"x", "1"}}));

    show("legacy", call_named<^^legacy>({{"code", "21"}}));
    show("legacy missing", call_named<^^legacy>({}));
}
```

It must print exactly:

```
add: [5]
divide: [5]
divide negative: [-3]
add missing: nullopt
add unknown: nullopt
add bad int: nullopt
add empty int: nullopt
greet: [ann]
greet times: [ann ann ann]
greet all: [ann ann!]
greet false: [bo]
greet gap: nullopt
greet bad bool: nullopt
greet no name: nullopt
greet empty name: []
scale: [5]
scale factor: [4.5]
scale bad: nullopt
counter: [1001]
counter start: [701]
counter step: nullopt
counter both: [705]
answer: [42]
answer extra: nullopt
legacy: [42]
legacy missing: nullopt
```

Write the complete header to `solution.hpp` in the current directory.
