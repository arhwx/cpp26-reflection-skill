Some of our structs are stored by the million, and their members are declared in an order that reads well but wastes space on padding. I don't want to reorder the source, so I want a generated twin with the members sorted by alignment, plus conversions between the two.

API:
- `compact_t<T>`: an aggregate struct type with one member for each non-static data member of `T`, with the same name and type. The members are declared in order of decreasing alignment, where a member's alignment is `alignof` of its type. Members with equal alignment keep their order from `T`. Naming `compact_t<T>` again for the same `T` gives the same type.
- `template <class T> compact_t<T> to_compact(const T& value)`: copies every member of `value` to the member of the result with the same name.
- `template <class T> T from_compact(const compact_t<T>& value)`: the reverse conversion.
- `template <class T> constexpr std::array<std::string_view, N> compact_names()` (`consteval` is fine too): the member names of `compact_t<T>` in declaration order, where `N` is the number of members.

`T` is always an aggregate whose members are all public, with no base classes, bit-fields, arrays, references or const members. Every member type is default constructible and copyable. The expected output assumes the alignments of x86-64 Linux.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <cstdint>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>

struct Rgb {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

struct alignas(16) Vec4 {
    float x;
    float y;
    float z;
    float w;
};

struct Record {
    char kind;
    double score;
    Rgb color;
    short port;
    std::string name;
    bool active;
    Vec4 pos;
    int count;
    std::int64_t id;
};

struct Sample {
    char unit;
    double temp;
    int id;
    short raw0;
    double pressure;
    char flag;
    int count;
    short raw1;
    double humidity;
    bool valid;
    int seq;
    short raw2;
    double voltage;
    char mode;
    int errors;
    short raw3;
    float gain;
    char zone;
};

struct Empty {};

static_assert(std::is_aggregate_v<compact_t<Record>>);
static_assert(!std::is_same_v<compact_t<Record>, Record>);
static_assert(std::is_same_v<decltype(compact_t<Record>::name), std::string>);
static_assert(std::is_same_v<decltype(compact_t<Record>::color), Rgb>);
static_assert(std::is_same_v<decltype(compact_t<Sample>::gain), float>);
static_assert(compact_names<Record>().size() == 9 && compact_names<Record>()[0] == "pos");
static_assert(compact_names<Empty>().empty());

template <class T>
void print_names(std::string_view label) {
    std::print("{}:", label);
    for (std::string_view name : compact_names<T>())
        std::print(" {}", name);
    std::println();
}

int main() {
    print_names<Record>("Record");
    print_names<Sample>("Sample");
    print_names<Empty>("Empty");
    std::println("not larger: {}", sizeof(compact_t<Record>) <= sizeof(Record));

    Record r{'k', 2.5, {1, 2, 3}, 8080, "node", true, {1, 2, 3, 4}, 42, 7'000'000'000};
    compact_t<Record> c = to_compact(r);
    std::println("{} {} {} {} {} {} {} {} {}", c.pos.w, c.score, c.name, c.id, c.count, c.port, c.kind, c.color.b,
                 c.active);

    // designated initializers must follow the member order of compact_t<Record>
    compact_t<Record> d{.pos = {0.5f, 0, 0, 0}, .score = -1.5, .name = "edge", .id = -9, .count = 3, .port = 22,
                       .kind = 'e', .color = {9, 8, 7}, .active = false};
    Record back = from_compact<Record>(d);
    std::println("{} {} {} {} {} {} {} {} {}", back.kind, back.score, back.color.r, back.port, back.name, back.active,
                 back.pos.x, back.count, back.id);

    Sample s{'C', 21.5, 1, 10, 1013.25, 'f', 5, 11, 40.5, true, 77, 12, 3.25, 'm', 0, 13, 0.75f, 'z'};
    Sample t = from_compact<Sample>(to_compact(s));
    std::println("{} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {} {}", t.unit, t.temp, t.id, t.raw0, t.pressure,
                 t.flag, t.count, t.raw1, t.humidity, t.valid, t.seq, t.raw2, t.voltage, t.mode, t.errors, t.raw3,
                 t.gain, t.zone);
}
```

It must print exactly:

```
Record: pos score name id count port kind color active
Sample: temp pressure humidity voltage id count seq errors gain raw0 raw1 raw2 raw3 unit flag valid mode zone
Empty:
not larger: true
4 2.5 node 7000000000 42 8080 k 3 true
e -1.5 9 22 edge false 0.5 3 -9
C 21.5 1 10 1013.25 f 5 11 40.5 true 77 12 3.25 m 0 13 0.75 z
```

Write the complete header to `solution.hpp` in the current directory.
