I want a small JSON writer for flat structs, where individual fields can be renamed or left out by annotating them in the struct definition.

Everything goes in `namespace json`:
- `json::rename("key")`: annotation that writes the member under a different key, used as `[[=json::rename("user_id")]] int id;`.
- `json::skip`: annotation that leaves the member out, used as `[[=json::skip]] std::string password;`.
- `template <class T> std::string json::to_json(const T& value)`: writes `value` as a JSON object.

Output rules:
- Members in declaration order, no whitespace: `{"key":value,"key":value}`. A struct with nothing to write gives `{}`.
- Members are `int` (written as a number) or `std::string` (written in double quotes, no escaping needed).
- Members may also carry annotations from other libraries, like `[[=db::column{4}]]` in the test. Those must be ignored.

The annotation spellings must compile exactly as they appear in the test.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>

namespace db {
struct column {
    int index;
};
}

struct User {
    [[=json::rename("user_id")]] int id;
    std::string name;
    [[=json::skip]] std::string password;
    [[=db::column{4}]] int age;
    [[=json::rename("e-mail")]] std::string email;
};

struct Point {
    int x;
    int y;
};

struct Secret {
    [[=json::skip]] int key;
};

int main() {
    User u{42, "ann", "swordfish", 31, "ann@example.com"};
    std::println("{}", json::to_json(u));
    std::println("{}", json::to_json(Point{-3, 7}));
    std::println("{}", json::to_json(Secret{1234}));
}
```

It must print exactly:

```
{"user_id":42,"name":"ann","age":31,"e-mail":"ann@example.com"}
{"x":-3,"y":7}
{}
```

Write the complete header to `solution.hpp` in the current directory.
