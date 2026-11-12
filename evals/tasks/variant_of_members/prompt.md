Our debug console reads a member name at run time and should show that member of an object. I want a generic way to get a member by name, with the value returned as a `std::variant`.

API:
- `template <class T> using members_variant_t = ...;`: a `std::variant` whose alternatives are the types of the public non-static data members of `T`, without `const`. Each type appears once, in the order of its first appearance among the members. A type with no public non-static data members gives `std::variant<std::monostate>`.
- `template <class T> std::optional<members_variant_t<T>> get_member(const T& obj, std::string_view name)`: a copy of the public non-static data member of `obj` named `name` (exact comparison), held as the alternative for that member's type. If there is no such member, it returns `std::nullopt`.

Static data members, member functions and private members are never found. Members are never references or arrays.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

using Celsius = double;

struct Reading {
    int id;
    double value;
    std::string unit;
    int count;
    const int version = 1;
    Celsius limit;
    bool valid;
    std::string source;
    static inline long total = 0;
};

class Sensor {
public:
    Sensor(std::string name, long ticks) : name(std::move(name)), ticks(ticks) {}
    double calibrated() const { return offset_ + drift_; }

    std::string name;
    long ticks;
    char grade = 'B';

private:
    double offset_ = 0.5;
    float drift_ = 0.25f;
};

class Hidden {
public:
    int get() const { return secret_; }

private:
    int secret_ = 7;
};

struct Empty {};

static_assert(std::is_same_v<members_variant_t<Reading>, std::variant<int, double, std::string, bool>>);
static_assert(std::is_same_v<members_variant_t<Sensor>, std::variant<std::string, long, char>>);
static_assert(std::is_same_v<members_variant_t<Hidden>, std::variant<std::monostate>>);
static_assert(std::is_same_v<members_variant_t<Empty>, std::variant<std::monostate>>);
static_assert(std::is_same_v<decltype(get_member(std::declval<const Reading&>(), "id")),
                             std::optional<members_variant_t<Reading>>>);

template <class T>
void show(const T& obj, std::string_view name) {
    std::optional<members_variant_t<T>> v = get_member(obj, name);
    if (!v) {
        std::println("{}: missing", name);
        return;
    }
    std::visit([&](const auto& x) { std::println("{}: {} (alternative {})", name, x, v->index()); }, *v);
}

int main() {
    const Reading r{1, 21.5, "C", 4, 3, 30.0, true, "lab"};
    const std::string_view reading_names[] = {"id",    "value",  "unit",  "count", "version", "limit",
                                              "valid", "source", "total", "Id",    ""};
    for (std::string_view name : reading_names)
        show(r, name);

    Sensor s("probe", 1200);
    const std::string_view sensor_names[] = {"name", "ticks", "grade", "offset_", "calibrated"};
    for (std::string_view name : sensor_names)
        show(s, name);
    std::println("calibrated {}", s.calibrated());

    std::println("hidden {} {}", get_member(Hidden{}, "secret_").has_value(), Hidden{}.get());
    std::println("empty {}", get_member(Empty{}, "").has_value());

    std::optional<members_variant_t<Reading>> unit = get_member(r, "unit");
    std::println("unit is string {}", std::holds_alternative<std::string>(*unit));
    std::optional<members_variant_t<Reading>> version = get_member(r, "version");
    std::println("version {}", std::get<int>(*version) + 1);
}
```

It must print exactly:

```
id: 1 (alternative 0)
value: 21.5 (alternative 1)
unit: C (alternative 2)
count: 4 (alternative 0)
version: 3 (alternative 0)
limit: 30 (alternative 1)
valid: true (alternative 3)
source: lab (alternative 2)
total: missing
Id: missing
: missing
name: probe (alternative 0)
ticks: 1200 (alternative 1)
grade: B (alternative 2)
offset_: missing
calibrated: missing
calibrated 0.75
hidden false 7
empty false
unit is string true
version 4
```

Write the complete header to `solution.hpp` in the current directory.
