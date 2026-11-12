We pack message headers into as few bytes as possible before they go into a ring buffer. Today every header struct has a hand-written twin with bit-fields and two conversion functions, and they keep drifting apart. I want to write the widths on the original struct and have the packed twin generated.

Everything goes in `namespace pack`:
- `pack::bits(n)`: annotation that gives a member a width of `n` bits, used as `[[=pack::bits(3)]] unsigned char version;`. It can also come from a constant, like `nibble` in the test.
- `pack::packed_t<T>`: an aggregate struct type with one member for each non-static data member of `T`, with the same name, type and order. A member annotated with `pack::bits(n)` becomes a bit-field of width `n`. A member that is already a bit-field in `T` stays a bit-field of the same width. All other members are ordinary members. Naming `packed_t<T>` again for the same `T` gives the same type.
- `template <class T> pack::packed_t<T> pack::pack(const T& value)`: copies every member of `value` to the member of the result with the same name. An annotated member is masked to its width: only its lowest `n` bits are kept, so 9 in a 3-bit member becomes 1.
- `template <class T> T pack::unpack(const pack::packed_t<T>& value)`: copies every member back.
- `template <class T> constexpr int pack::bit_width(std::string_view name)` (`consteval` is fine too): the width of the member `name` of `packed_t<T>` if it is a bit-field, otherwise 0. A name that `T` does not have also gives 0.

Rules:
- Annotated members have an unsigned integer type or `bool`, and `n` is at least 1 and at most the number of bits of that type.
- Members may also carry annotations from other libraries, like `audit::note` in the test. Those must be ignored.
- `T` is always an aggregate whose members are all public.

The annotation spellings must compile exactly as they appear in the test.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <cstdint>
#include <print>
#include <string>
#include <type_traits>

namespace audit {
struct note {
    int level;
};
}  // namespace audit

constexpr pack::bits nibble(4);

struct Header {
    [[=pack::bits(3)]] unsigned char version;
    [[=pack::bits(1)]] bool urgent;
    [[=audit::note{2}]] [[=pack::bits(12)]] std::uint16_t length;
    [[=nibble]] unsigned channel;
    [[=pack::bits(40)]] std::uint64_t stamp;
    unsigned legacy : 5;
    std::uint32_t crc;
    [[=audit::note{1}]] std::string tag;
    double weight;
};

// a bit-field has no address
template <class P>
concept version_addressable = requires(P p) { &p.version; };
template <class P>
concept legacy_addressable = requires(P p) { &p.legacy; };
template <class P>
concept crc_addressable = requires(P p) { &p.crc; };

static_assert(version_addressable<Header> && !version_addressable<pack::packed_t<Header>>);
static_assert(!legacy_addressable<pack::packed_t<Header>>);
static_assert(crc_addressable<pack::packed_t<Header>>);
static_assert(std::is_same_v<decltype(pack::packed_t<Header>::version), unsigned char>);
static_assert(std::is_same_v<decltype(pack::packed_t<Header>::stamp), std::uint64_t>);
static_assert(std::is_same_v<decltype(pack::packed_t<Header>::tag), std::string>);

static_assert(pack::bit_width<Header>("version") == 3);
static_assert(pack::bit_width<Header>("urgent") == 1);
static_assert(pack::bit_width<Header>("length") == 12);
static_assert(pack::bit_width<Header>("channel") == 4);
static_assert(pack::bit_width<Header>("stamp") == 40);
static_assert(pack::bit_width<Header>("legacy") == 5);
static_assert(pack::bit_width<Header>("crc") == 0);
static_assert(pack::bit_width<Header>("tag") == 0);
static_assert(pack::bit_width<Header>("size") == 0);

void print(const Header& h) {
    std::println("version={} urgent={} length={} channel={} stamp={} legacy={} crc={} tag={} weight={}", h.version,
                 h.urgent, h.length, h.channel, h.stamp, +h.legacy, h.crc, h.tag, h.weight);
}

int main() {
    Header h{5, true, 1500, 9, 1'000'000'000'000, 17, 48879, "ok", 0.5};
    pack::packed_t<Header> p = pack::pack(h);
    std::uint64_t stamp = p.stamp;
    std::println("packed version={} length={} stamp={} crc={}", +p.version, +p.length, stamp, p.crc);
    print(pack::unpack<Header>(p));

    // values that do not fit are masked to the width
    Header big{9, false, 5000, 31, 1'200'000'000'000, 31, 70000, "big", -2.0};
    print(pack::unpack<Header>(pack::pack(big)));

    pack::packed_t<Header> q{.version = 7, .urgent = true, .length = 4095, .channel = 15, .stamp = 1'099'511'627'775,
                             .legacy = 31, .crc = 1, .tag = "max", .weight = 1.5};
    print(pack::unpack<Header>(q));
}
```

It must print exactly:

```
packed version=5 length=1500 stamp=1000000000000 crc=48879
version=5 urgent=true length=1500 channel=9 stamp=1000000000000 legacy=17 crc=48879 tag=ok weight=0.5
version=1 urgent=false length=904 channel=15 stamp=100488372224 legacy=31 crc=70000 tag=big weight=-2
version=7 urgent=true length=4095 channel=15 stamp=1099511627775 legacy=31 crc=1 tag=max weight=1.5
```

Write the complete header to `solution.hpp` in the current directory.
