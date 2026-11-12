I'm writing a small request router. A handler is an ordinary class, and each endpoint is a member function marked with the path it serves. Adding an endpoint should only mean writing the function and its marker.

Everything goes in `namespace web`:
- `web::route("/path")`: annotation that makes a member function serve a path, used as `[[=web::route("/users")]] int list_users();`. A function may serve several paths, as `health` does in the test.
- `template <class Handler> int web::handle(Handler& h, std::string_view path)`: finds the public non-static member function of `Handler` that has a `web::route` equal to `path` (exact comparison), calls it on `h` with no arguments and returns its result. If there is none, it returns -1 and calls nothing.

Rules:
- Routed functions take no parameters and return `int`. Some are `const`.
- Only public non-static member functions are routed. A `web::route` on a static member function, a private member function or a data member is ignored.
- Members may also carry annotations from other libraries, like `audit::log` in the test. Those must be ignored.
- No path appears twice in one class.

The annotation spellings must compile exactly as they appear in the test.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string_view>

namespace audit {
struct log {
    int level;
};
}  // namespace audit

class Api {
public:
    Api() = default;
    explicit Api(int base) : base_(base) {}
    Api(const Api&) = default;
    Api& operator=(const Api&) = default;
    ~Api() {}

    bool operator==(const Api&) const = default;

    [[=web::route("/")]] int operator()() const { return base_ * 2; }

    [[=web::route("/users")]] int list_users() { return base_ + 1; }
    [[=web::route("/users/count")]] int count_users() const { return 3; }
    [[=audit::log{2}]] [[=web::route("/health"), =web::route("/status")]] int health() {
        ++hits;
        return 200;
    }
    [[=audit::log{1}]] int audited() { return 7; }
    int unrouted() { return 99; }
    [[=web::route("/version")]] static int version() { return 2; }

    [[=web::route("/hits")]] int hits = 0;

private:
    [[=web::route("/secret")]] int secret() { return 42; }
    int base_ = 10;
};

struct Empty {
    int value = 5;
    int get() { return value; }
};

int main() {
    Api api(20);
    const std::string_view paths[] = {"/users", "/users/count", "/health", "/status", "/", "/users/",
                                      "/version", "/hits", "/secret", "audited", "", "/nope"};
    for (std::string_view path : paths)
        std::println("{} -> {}", path, web::handle(api, path));
    std::println("hits {}", api.hits);

    Api fresh;
    std::println("fresh / -> {}", web::handle(fresh, "/"));

    Empty e;
    std::println("empty -> {}", web::handle(e, "/users"));
}
```

It must print exactly:

```
/users -> 21
/users/count -> 3
/health -> 200
/status -> 200
/ -> 40
/users/ -> -1
/version -> -1
/hits -> -1
/secret -> -1
audited -> -1
 -> -1
/nope -> -1
hits 2
fresh / -> 20
empty -> -1
```

Write the complete header to `solution.hpp` in the current directory.
