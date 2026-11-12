Our services accept partial configuration updates: a client sends only the fields it wants to change. Today every config struct has a hand-written twin where each field is a `std::optional`, and the twins keep falling out of sync. I want the patch type generated from the struct.

API:
- `template <class T> using patch_of = ...;`: an aggregate struct with one member for each non-static data member of `T`, with the same name and in the same order, whose type is `std::optional<M>` where `M` is the type of the original member. A default-constructed patch has every member disengaged. Naming `patch_of<T>` again for the same `T` gives the same type.
- `template <class T> void apply_patch(T& target, const patch_of<T>& patch)`: for each engaged member of `patch`, assigns its value to the member of `target` with the same name. All other members of `target` stay unchanged.

`T` is always an aggregate whose members are all public.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <type_traits>
#include <vector>

struct Config {
    std::string host = "localhost";
    int port = 8080;
    bool verbose = false;
    double timeout = 1.5;
    std::vector<int> retries{1, 2};
};

struct Point {
    int x;
    int y;
};

static_assert(std::is_same_v<decltype(patch_of<Config>::port), std::optional<int>>);
static_assert(std::is_same_v<decltype(patch_of<Config>::host), std::optional<std::string>>);
static_assert(std::is_same_v<decltype(patch_of<Config>::retries), std::optional<std::vector<int>>>);
static_assert(std::is_same_v<decltype(patch_of<Point>::y), std::optional<int>>);
static_assert(!std::is_same_v<patch_of<Config>, patch_of<Point>>);

void print(const Config& c) {
    std::println("{}:{} verbose={} timeout={} retries={}", c.host, c.port, c.verbose, c.timeout, c.retries.size());
}

int main() {
    Config cfg;
    print(cfg);

    patch_of<Config> p;
    std::println("engaged {} {} {} {} {}", p.host.has_value(), p.port.has_value(), p.verbose.has_value(),
                 p.timeout.has_value(), p.retries.has_value());
    p.port = 9000;
    apply_patch(cfg, p);
    print(cfg);

    patch_of<Config> q;
    q.host = "example.org";
    q.verbose = true;
    q.retries = std::vector<int>{};
    apply_patch(cfg, q);
    print(cfg);

    apply_patch(cfg, patch_of<Config>{});
    print(cfg);

    Point pt{1, 2};
    apply_patch(pt, patch_of<Point>{.x = {}, .y = -7});
    std::println("{} {}", pt.x, pt.y);
    apply_patch(pt, patch_of<Point>{3, std::nullopt});
    std::println("{} {}", pt.x, pt.y);
}
```

It must print exactly:

```
localhost:8080 verbose=false timeout=1.5 retries=2
engaged false false false false false
localhost:9000 verbose=false timeout=1.5 retries=2
example.org:9000 verbose=true timeout=1.5 retries=0
example.org:9000 verbose=true timeout=1.5 retries=0
1 -7
3 -7
```

Write the complete header to `solution.hpp` in the current directory.
