Our log lines and metrics keys should carry the fully qualified name of the type, function or value they refer to. Right now people type these names by hand and they drift after every rename, so I want them computed from the declarations.

API:
- `template <std::meta::info R> std::string_view qualified_name()`: `R` reflects a class, enumeration, enumerator, function or variable declared at namespace scope (for an enumerator: its enumeration is declared at namespace scope). It returns the enclosing namespaces from outermost to innermost, then the name of the entity itself, all separated by `::`. There is no leading `::`, so an entity in the global namespace gives just its own name.

Naming rules:
- An enumerator is always named through its enumeration, for scoped and unscoped enumerations alike: `net::Kind::kind_a`, even though `net::kind_a` also refers to it.
- Namespace aliases never appear in a name. An entity reached through an alias gives the names of the namespaces it was really declared in.

`qualified_name` must work in constant expressions (the test `static_assert`s on it) and at run time. The returned view must stay valid for the whole program.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <meta>
#include <print>
#include <string_view>

struct Global {
    int id;
};

int answer() { return 42; }

namespace net {
enum class Proto { tcp, udp };
enum Kind { kind_a, kind_b };
inline constexpr int default_port = 80;

namespace http {
struct Request {
    int id;
};
enum class Method { get, post };
int parse(std::string_view text) { return static_cast<int>(text.size()); }
}  // namespace http
}  // namespace net

namespace a::b::c {
struct Deep {};
int counter = 0;
}  // namespace a::b::c

namespace nh = net::http;

static_assert(qualified_name<^^net::Proto::tcp>() == "net::Proto::tcp");
static_assert(qualified_name<^^Global>() == "Global");
static_assert(qualified_name<^^nh::Request>().size() == 18);

template <std::meta::info R>
void show() {
    constexpr std::string_view name = qualified_name<R>();
    std::string_view at_run_time = qualified_name<R>();
    std::println("{} ({}, {})", at_run_time, at_run_time.size(), name == at_run_time);
}

int main() {
    show<^^Global>();
    show<^^answer>();
    show<^^net::Proto>();
    show<^^net::Proto::udp>();
    show<^^net::Kind>();
    show<^^net::kind_b>();
    show<^^net::Kind::kind_a>();
    show<^^net::default_port>();
    show<^^net::http::Request>();
    show<^^nh::Method::post>();
    show<^^net::http::parse>();
    show<^^a::b::c::Deep>();
    show<^^a::b::c::counter>();
}
```

It must print exactly:

```
Global (6, true)
answer (6, true)
net::Proto (10, true)
net::Proto::udp (15, true)
net::Kind (9, true)
net::Kind::kind_b (17, true)
net::Kind::kind_a (17, true)
net::default_port (17, true)
net::http::Request (18, true)
net::http::Method::post (23, true)
net::http::parse (16, true)
a::b::c::Deep (13, true)
a::b::c::counter (16, true)
```

Write the complete header to `solution.hpp` in the current directory.
