For a binary protocol tool I want to print and check the memory layout of our message structs.

API:
- `struct field_info { std::string_view name; std::size_t offset; std::size_t size; };` defined in the header exactly like this.
- `template <class T> constexpr auto member_offsets()`: returns a `std::array<field_info, N>` with one entry per non-static data member of `T`, in declaration order. `name` is the member name, `offset` its offset in bytes from the start of `T` (what `offsetof` gives), and `size` its `sizeof`.

It must be usable in constant expressions: the test checks it against `offsetof` and `sizeof` with `static_assert`s. It must work for any struct, without listing the members by hand.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <cstddef>
#include <cstdint>
#include <print>

struct Packet {
    std::uint8_t kind;
    std::uint32_t length;
    std::uint16_t flags;
    double stamp;
    char tag[6];
    std::uint64_t checksum;
};

struct Pair {
    short a;
    short b;
};

constexpr auto packet = member_offsets<Packet>();
static_assert(packet.size() == 6);
static_assert(packet[0].name == "kind" && packet[0].offset == offsetof(Packet, kind));
static_assert(packet[1].offset == offsetof(Packet, length) && packet[1].size == sizeof(std::uint32_t));
static_assert(packet[2].offset == offsetof(Packet, flags));
static_assert(packet[3].offset == offsetof(Packet, stamp) && packet[3].size == sizeof(double));
static_assert(packet[4].name == "tag" && packet[4].size == sizeof(Packet::tag));
static_assert(packet[5].offset == offsetof(Packet, checksum));
static_assert(member_offsets<Pair>()[1].offset == offsetof(Pair, b));

int main() {
    for (const field_info& f : packet)
        std::println("{:<9}{:>4}{:>4}", f.name, f.offset, f.size);
    for (const field_info& f : member_offsets<Pair>())
        std::println("{:<9}{:>4}{:>4}", f.name, f.offset, f.size);
}
```

It must print exactly:

```
kind        0   1
length      4   4
flags       8   2
stamp      16   8
tag        24   6
checksum   32   8
a           0   2
b           2   2
```

Write the complete header to `solution.hpp` in the current directory.
