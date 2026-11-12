For our wire protocol structs I want a `pahole`-style layout report that shows every field, hole and padding byte, and a layout hash I can pin with `static_assert` so that an accidental layout change breaks the build.

API, all in `namespace layout`:
- `template <class T> std::string report()`: the layout report of `T`, as described below.
- `template <class T> constexpr std::string_view signature`: a variable template holding the layout signature of `T`, usable in constant expressions.
- `template <class T> constexpr std::uint64_t hash`: a variable template holding the 64-bit FNV-1a hash of `signature<T>`, usable in constant expressions.

It must work for any struct or class without listing members by hand. The test types have no virtual functions, no virtual bases and no `[[no_unique_address]]` members.

Fields:
- The fields of `T` are, in this order: the fields of each direct base class, in base declaration order, then the fields contributed by each non-static data member of `T`, in declaration order. Private and protected members count too.
- A base class contributes its own fields, found with these same rules, with its unqualified name and `::` put in front of each name. An empty base contributes nothing.
- A non-static data member whose type is a class type (a struct or class, not a union) contributes that class's fields, found with these same rules, with `<member name>.` put in front of each name. So a base `B` of a member `m` gives names like `m.B::x`.
- Every other non-static data member (scalars, unions, arrays of anything, bit-fields) is one field named by its member name.
- The start of a field is its position in bits from the start of the `T` object: 8 times its byte offset, plus its bit offset inside that byte for a bit-field. Its length in bits is the bit-field width for a bit-field, and 8 times its `sizeof` otherwise.

Report: each line ends with a newline, and items on a line are separated by single spaces.
- The first line is `<name> size=<sizeof(T)> align=<alignof(T)>`, where `<name>` is the unqualified name of `T`.
- A bit position `p` is printed as `p / 8` when `p` is a multiple of 8, and as `<p / 8>.<p % 8>` otherwise, so bit 131 is `16.3`.
- Then one line per field, in field order: two spaces, the position of its start, its name, and `bits=<width>` for a bit-field or `size=<sizeof>` otherwise.
- Holes: walk the fields in order, keeping `end`, the bit just past the previous field (0 before the first field). When a field starts after `end`, print a hole line before its field line: two spaces, the position of `end`, `hole`, and the extent of the gap.
- Padding: when `8 * sizeof(T)` is greater than `end` after the last field, print two spaces, the position of `end`, `padding`, and the extent of the gap up to `8 * sizeof(T)`.
- The extent of a gap of `n` bits starting at bit `p` is `size=<n / 8>` when both `p` and `n` are multiples of 8, and `bits=<n>` otherwise.
- The last line is `  fields=<number of fields> holes=<number of hole lines> unused_bits=<total bits of all holes and the padding>`.

Signature and hash:
- The signature is, for each field in field order, `<name>@<start>:<length>;` with start and length in bits as decimal numbers, followed by `=<sizeof(T)>/<alignof(T)>`. There are no spaces.
- The hash is 64-bit FNV-1a over the bytes of the signature, without any terminator: start with `0xcbf29ce484222325`; for each byte, xor the byte (as an unsigned value from 0 to 255) into the hash, then multiply by `0x100000001b3` modulo 2^64.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <cstdint>
#include <print>

struct Header {
    std::uint8_t version;
    std::uint32_t length;
    std::uint16_t kind;
};

struct Tagged {};

struct Base {
    std::uint16_t id;
};

class Message : public Tagged, public Base {
public:
    Header head;
    std::uint8_t prio : 3;
    std::uint8_t urgent : 1;
    std::uint32_t seq : 20;
    char name[5];
    double score;

private:
    bool sealed_ = false;
};

struct Bits {
    std::uint16_t low : 3;
    std::uint16_t mid : 4;
};

union Number {
    std::int64_t i;
    double d;
};

struct Cell {
    Bits flags;
    Number value;
    Header history[2];
};

namespace v1 {
struct Record { std::uint32_t id; std::uint16_t flags; std::uint16_t size; };
}
namespace v2 {  // other member types of the same sizes
struct Record { float id; std::int16_t flags; char size[2]; };
}
namespace v3 {  // members reordered
struct Record { std::uint16_t flags; std::uint32_t id; std::uint16_t size; };
}
namespace v4 {  // a member renamed
struct Record { std::uint32_t id; std::uint16_t flags; std::uint16_t bytes; };
}

// the abi guard: both must hold at compile time
static_assert(layout::signature<v1::Record> == "id@0:32;flags@32:16;size@48:16;=8/4");
static_assert(layout::hash<v1::Record> == 0xfd1f39e88010ac25);

template <class A, class B>
void compare(const char* what) {
    std::println("{}: {}", what, layout::hash<A> == layout::hash<B> ? "same layout" : "layout changed");
}

int main() {
    std::print("{}", layout::report<Header>());
    std::print("{}", layout::report<Message>());
    std::print("{}", layout::report<Bits>());
    std::print("{}", layout::report<Cell>());
    std::print("{}", layout::report<v3::Record>());

    std::println("signature of Message: {}", layout::signature<Message>);
    std::println("signature of Cell: {}", layout::signature<Cell>);
    std::println("hash of Message: {:#018x}", layout::hash<Message>);
    std::println("hash of Bits: {:#018x}", layout::hash<Bits>);

    compare<v1::Record, v2::Record>("v2 other types");
    compare<v1::Record, v3::Record>("v3 reordered");
    compare<v1::Record, v4::Record>("v4 renamed");
    compare<Header, Header>("Header with itself");
}
```

It must print exactly:

```
Header size=12 align=4
  0 version size=1
  1 hole size=3
  4 length size=4
  8 kind size=2
  10 padding size=2
  fields=3 holes=1 unused_bits=40
Message size=40 align=8
  0 Base::id size=2
  2 hole size=2
  4 head.version size=1
  5 hole size=3
  8 head.length size=4
  12 head.kind size=2
  14 hole size=2
  16 prio bits=3
  16.3 urgent bits=1
  16.4 seq bits=20
  19 name size=5
  24 score size=8
  32 sealed_ size=1
  33 padding size=7
  fields=10 holes=3 unused_bits=112
Bits size=2 align=2
  0 low bits=3
  0.3 mid bits=4
  0.7 padding bits=9
  fields=2 holes=0 unused_bits=9
Cell size=40 align=8
  0 flags.low bits=3
  0.3 flags.mid bits=4
  0.7 hole bits=57
  8 value size=8
  16 history size=24
  fields=4 holes=1 unused_bits=57
Record size=12 align=4
  0 flags size=2
  2 hole size=2
  4 id size=4
  8 size size=2
  10 padding size=2
  fields=3 holes=1 unused_bits=32
signature of Message: Base::id@0:16;head.version@32:8;head.length@64:32;head.kind@96:16;prio@128:3;urgent@131:1;seq@132:20;name@152:40;score@192:64;sealed_@256:8;=40/8
signature of Cell: flags.low@0:3;flags.mid@3:4;value@64:64;history@128:192;=40/8
hash of Message: 0x85a2a275624d190f
hash of Bits: 0x5bb06c2314997c2f
v2 other types: same layout
v3 reordered: layout changed
v4 renamed: layout changed
Header with itself: same layout
```

Write the complete header to `solution.hpp` in the current directory.
