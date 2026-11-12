Our game server sends player state to clients. Today every replicated struct has a hand-written snapshot struct and hand-written delta code next to it. I want to mark the members to replicate with an annotation and have the snapshot struct and the delta encoding generated.

Everything goes in `namespace net`:
- `net::replicated`: annotation that marks a non-static data member for replication, used as `[[=net::replicated]]`.
- `net::snapshot_t<T>`: an aggregate struct type with one public member for each replicated member of `T`, with the same name and type, in declaration order. It has no other members. Naming `snapshot_t<T>` again for the same `T` gives the same type.
- `net::snapshot(obj)`: returns a `snapshot_t<T>` holding copies of the replicated members of `obj`.
- `net::restore(obj, snap)`: copies every member of `snap` back into the replicated member of `obj` with the same name. The other members of `obj` are not touched.
- `net::encode(cur)` and `net::encode(prev, cur)`, where `prev` and `cur` are both `snapshot_t<T>` for the same `T`, deduced from the arguments: return a `std::vector<std::uint8_t>`, a full update and a delta update as defined below.
- `net::apply(base, delta)`, where `base` is a `snapshot_t<T>` and `delta` converts to `std::span<const std::uint8_t>` (the test passes vectors): decodes an update produced by `encode` and writes it into `base`. Returns `true` on success. On failure it returns `false` and leaves `base` unchanged.

Rules:
- Replicated members may be private, like those of `Door`. `snapshot` and `restore` read and write them anyway.
- Replicated members have type `bool`, an integer type, an enumeration, `float`, `double` or `std::string`.
- Members may also carry annotations from other libraries, like `audit::note` in the test. Those must be ignored.

Wire format. The members of `snapshot_t<T>` are fields numbered from 0 in declaration order; let `n` be their count.
- A field value is encoded as:
  - `bool`: one byte, 0 or 1.
  - integer: `sizeof` bytes of its two's complement value, least significant byte first.
  - enumeration: as its underlying integer type.
  - `float` and `double`: the 4 or 8 bytes of the IEEE bit pattern, least significant byte first.
  - `std::string`: the length as an unsigned LEB128 number (7 bits per byte, least significant group first, the high bit set on every byte except the last), then the bytes of the string.
- An update is a mask of `(n + 7) / 8` bytes, where field `i` is included when bit `i % 8` (value `1 << (i % 8)`) of byte `i / 8` is set, followed by the encodings of the included fields in field order. Unused mask bits are 0.
- `encode(cur)` includes every field.
- `encode(prev, cur)` includes exactly the fields whose encoding in `cur` differs from their encoding in `prev`. So `0.0` and `-0.0` differ, and a NaN with the same bits as before does not. If no field differs, the result is empty, with no mask.
- `apply` accepts an empty `delta` and changes nothing. Otherwise it fails if `delta` is shorter than the mask, if the mask sets a bit for a field number `n` or above, if the data ends inside a field, if bytes remain after the last included field, or if a `bool` byte is neither 0 nor 1.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <cmath>
#include <cstdint>
#include <print>
#include <string>
#include <type_traits>
#include <vector>

namespace audit {
struct note {
    int level;
};
}  // namespace audit

enum class Team : std::uint8_t { red, blue };
enum Stance : std::int16_t { standing = 0, crouching = 3, prone = -2 };

struct Player {
    [[=net::replicated]] std::uint32_t id = 0;
    [[=net::replicated]] std::string name;
    int local_frame = 0;
    [[=net::replicated]] Team team = Team::red;
    [[=net::replicated]] std::int16_t health = 100;
    [[=net::replicated]] float x = 0;
    [[=net::replicated]] float y = 0;
    [[=audit::note{2}]] std::string debug;
    [[=net::replicated]] double heading = 0;
    [[=net::replicated]] bool alive = true;
    [[=audit::note{1}]] [[=net::replicated]] std::int64_t score = 0;
    [[=net::replicated]] Stance stance = standing;
};

class Door {
public:
    void open(int angle) { open_ = true; angle_ = angle; }
    void print() const { std::println("door open={} angle={} cache={}", open_, angle_, cache_); }
    int cache_ = 0;

private:
    [[=net::replicated]] bool open_ = false;
    [[=net::replicated]] std::int8_t angle_ = 0;
};

using Snap = net::snapshot_t<Player>;
static_assert(std::is_same_v<Snap, net::snapshot_t<Player>>);
static_assert(std::is_aggregate_v<Snap>);
static_assert(std::is_same_v<decltype(Snap::health), std::int16_t>);
static_assert(std::is_same_v<decltype(Snap::name), std::string>);
static_assert(std::is_same_v<decltype(net::snapshot_t<Door>::angle_), std::int8_t>);

template <class S>
concept has_local_frame = requires(S s) { s.local_frame; };
template <class S>
concept has_debug = requires(S s) { s.debug; };

void dump(const char* what, const std::vector<std::uint8_t>& bytes) {
    std::print("{} ({} bytes):", what, bytes.size());
    for (std::size_t i = 0; i < bytes.size() && i < 24; ++i) std::print(" {:02x}", bytes[i]);
    std::println("{}", bytes.size() > 24 ? " ..." : "");
}

void show(const Player& p) {
    std::println("player id={} name={} team={} health={} x={} y={} heading={} alive={} score={} stance={} frame={}",
                 p.id, p.name, static_cast<int>(p.team), p.health, p.x, p.y, p.heading, p.alive, p.score,
                 static_cast<int>(p.stance), p.local_frame);
}

int main() {
    std::println("snapshot has local_frame {}, debug {}", has_local_frame<Snap>, has_debug<Snap>);

    Player p{.id = 258, .name = "ann", .team = Team::blue, .x = 1.5f, .y = -2, .heading = 0.0, .score = 7};
    Snap s0 = net::snapshot(p);
    std::vector<std::uint8_t> full = net::encode(s0);
    dump("full", full);

    Snap client{};
    std::println("apply full: {}", net::apply(client, full));

    p.health = -5;
    p.x = 2.0f;
    p.local_frame = 99;
    Snap s1 = net::snapshot(p);
    std::vector<std::uint8_t> d1 = net::encode(s0, s1);
    dump("health and x", d1);

    p.score = -1;
    p.stance = prone;
    Snap s2 = net::snapshot(p);
    std::vector<std::uint8_t> d2 = net::encode(s1, s2);
    dump("score and stance", d2);

    p.debug = "only local";
    std::vector<std::uint8_t> d3 = net::encode(s2, net::snapshot(p));
    dump("local change", d3);

    p.heading = -0.0;
    p.y = NAN;
    Snap s3 = net::snapshot(p);
    std::vector<std::uint8_t> d4 = net::encode(s2, s3);
    dump("negative zero and nan", d4);
    std::println("nan again: {} bytes", net::encode(s3, net::snapshot(p)).size());

    std::println("apply d1: {}", net::apply(client, d1));
    std::println("apply d2: {}", net::apply(client, d2));
    std::println("apply d3: {}", net::apply(client, d3));

    std::vector<std::uint8_t> cut(d4.begin(), d4.end() - 1);
    std::println("apply truncated: {}", net::apply(client, cut));
    std::vector<std::uint8_t> extra = d2;
    extra.push_back(0);
    std::println("apply with extra byte: {}", net::apply(client, extra));
    std::vector<std::uint8_t> unknown = d2;
    unknown[1] |= 0x04;
    std::println("apply with unknown field: {}", net::apply(client, unknown));
    std::println("apply bad bool: {}", net::apply(client, std::vector<std::uint8_t>{0x80, 0x00, 0x02}));

    Player copy;
    copy.local_frame = 5;
    net::restore(copy, client);
    show(copy);
    std::println("apply d4: {}", net::apply(client, d4));
    net::restore(copy, client);
    std::println("heading signbit {}, y is nan {}", std::signbit(copy.heading), std::isnan(copy.y));

    p.name = std::string(200, 'z');
    std::vector<std::uint8_t> d5 = net::encode(s3, net::snapshot(p));
    dump("long name", d5);
    bool applied = net::apply(client, d5);
    std::println("apply long name: {}, size {}", applied, client.name.size());

    Door door;
    door.open(-90);
    door.cache_ = 3;
    auto ds = net::snapshot(door);
    dump("door", net::encode(ds));
    Door other;
    net::restore(other, ds);
    other.print();
}
```

It must print exactly:

```
snapshot has local_frame false, debug false
full (40 bytes): ff 03 02 01 00 00 03 61 6e 6e 01 64 00 00 00 c0 3f 00 00 00 c0 00 00 00 ...
apply full: true
health and x (8 bytes): 18 00 fb ff 00 00 00 40
score and stance (12 bytes): 00 03 ff ff ff ff ff ff ff ff fe ff
local change (0 bytes):
negative zero and nan (14 bytes): 60 00 00 00 c0 7f 00 00 00 00 00 00 00 80
nan again: 0 bytes
apply d1: true
apply d2: true
apply d3: true
apply truncated: false
apply with extra byte: false
apply with unknown field: false
apply bad bool: false
player id=258 name=ann team=1 health=-5 x=2 y=-2 heading=0 alive=true score=-1 stance=-2 frame=5
apply d4: true
heading signbit true, y is nan true
long name (204 bytes): 02 00 c8 01 7a 7a 7a 7a 7a 7a 7a 7a 7a 7a 7a 7a 7a 7a 7a 7a 7a 7a 7a 7a ...
apply long name: true, size 200
door (3 bytes): 03 01 a6
door open=true angle=-90 cache=0
```

Write the complete header to `solution.hpp` in the current directory.
