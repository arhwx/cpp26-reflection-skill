Our command handlers are plain functions in `namespace commands`. Every one of them takes some `int`s and returns an `int`. I want to call them by name from a script interpreter, and adding a new command should not require touching the dispatcher.

The header is always included after `namespace commands` has been declared, as in the test below, so it can refer to `commands` directly.

API:
- `std::optional<int> dispatch(std::string_view name, std::span<const int> args)`: if `commands` has a function called `name` whose number of parameters equals `args.size()`, call it with `args[0]`, `args[1]`, ... and return the result. Otherwise return `std::nullopt` and call nothing.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include <optional>
#include <print>
#include <span>
#include <string_view>
#include <vector>

namespace commands {
int add(int a, int b) { return a + b; }
int mul(int a, int b) { return a * b; }
int neg(int a) { return -a; }
}  // namespace commands

#include "solution.hpp"

void run(std::string_view name, std::vector<int> args) {
    std::optional<int> r = dispatch(name, args);
    if (r)
        std::println("{} -> {}", name, *r);
    else
        std::println("{} -> nullopt", name);
}

int main() {
    run("add", {2, 3});
    run("mul", {4, 5});
    run("neg", {7});
    run("add", {1});
    run("neg", {1, 2});
    run("mul", {});
    run("sub", {1, 2});
    run("commands", {});

    const int fixed[] = {-6, 7};
    std::optional<int> r = dispatch("mul", std::span<const int>(fixed));
    std::println("fixed -> {}", r.value_or(0));
}
```

It must print exactly:

```
add -> 5
mul -> 20
neg -> -7
add -> nullopt
neg -> nullopt
mul -> nullopt
sub -> nullopt
commands -> nullopt
fixed -> -42
```

Write the complete header to `solution.hpp` in the current directory.
