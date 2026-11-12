I want Protocol Buffers without `protoc`: plain C++ structs with field numbers in annotations are the messages. The library encodes and decodes the proto3 binary wire format, and writes the `.proto` text from the same structs, so the schema and the code cannot drift apart.

Everything goes in `namespace pb`:
- `pb::field(n)`: annotation on a non-static data member, used as `[[=pb::field(1)]]`. It gives the member field number `n`. Members without it are not part of the message: they are not encoded, not decoded (they keep their value-initialized value) and not in the schema.
- `pb::zigzag`: annotation, used as `[[=pb::zigzag]]` next to `pb::field`, that encodes a signed integer field (or the elements of a repeated one) as `sint32`/`sint64`.
- `template <class T> std::string pb::encode(const T& message)`: the wire bytes of `message`.
- `template <class T> std::optional<T> pb::decode(std::string_view bytes)`: the decoded message, or `std::nullopt` if the bytes are malformed.
- `template <class T> std::string pb::schema()`: the `.proto` text for `T`.
- `consteval std::string_view pb::check(std::meta::info type)`: `ok` if `type` is a valid message, otherwise its first problem.
- `encode`, `decode` and `schema` must not compile for a `T` that `check` rejects, but the test does not do that.

Types. On the test platform `int` is `std::int32_t`. A field (a member with a field number) has one of these types:
- a scalar: `bool`, `std::int32_t`, `std::int64_t`, `std::uint32_t`, `std::uint64_t`, `float`, `double`, `std::string`; an enum type (its values fit in `std::int32_t`); or a message, which is any other class type: an aggregate struct with public members.
- `std::vector<X>`: a repeated field, where `X` is a scalar, an enum or a message.
- `std::optional<X>`: a field with explicit presence, where `X` is a scalar, an enum or a message.

`check` looks only at the fields of `type` itself, in declaration order, and reports the first problem as `<T>.<member>: ` followed by the text below, where `<T>` is the unqualified name of the class. For each field, in this order:
- a number below 1 or above 536870911: `field number <n> is out of range`
- a number used by an earlier field: `field number <n> is already used by <earlier member>`
- a type that is not listed above: `unsupported type`
- `pb::zigzag` on a field whose type, or element type for `vector` and `optional`, is not `std::int32_t` or `std::int64_t`: `zigzag needs a signed integer`

Encoding (the proto3 wire format):
- A varint is an unsigned 64-bit value written 7 bits at a time, least significant group first; every byte except the last has its high bit set.
- A record is a tag, the varint `(number << 3) | wire_type`, followed by the value.
- Wire type 0, varint: `bool` (0 or 1), unsigned integers, signed integers and enums. A signed integer or enum value is sign-extended to 64 bits, so a negative value takes 10 bytes. With `pb::zigzag` the value is zigzag encoded at its own width instead, `(n << 1) ^ (n >> 31)` or `(n >> 63)`, so -1 is 1, 1 is 2, -2 is 3.
- Wire type 1: `double`, its 8 IEEE bytes, least significant first. Wire type 5: `float`, its 4 IEEE bytes, least significant first.
- Wire type 2, length-delimited, a varint length then that many bytes: `std::string` (its characters), a message (its encoding).
- The fields are written in declaration order.
- A scalar or enum field that is not `optional` or `vector` is left out when it is zero: `false`, `0`, `0.0`, an enum whose value is 0, or an empty string.
- A message field is always written, even when its encoding is empty.
- An `optional` field is written when it holds a value, whatever the value.
- A `vector` field with no elements is not written. A `vector` of numbers, `bool` or enums is packed: one wire type 2 record whose bytes are the values one after another, each without a tag. A `vector` of strings or messages writes one record per element, each with the tag, empty elements included.

Decoding:
- It starts from a value-initialized `T` and reads records until the end of the bytes. Any byte string that is a sequence of well-formed records is accepted, in any order.
- A field may be given more than once. For a scalar, enum or `optional` field the last record wins. For a message field, or an `optional` message that already holds a value, each record is decoded on top of the current value, so the fields of both records are merged. For a `vector` field each record appends.
- A `vector` of numbers, `bool` or enums accepts both a packed record (wire type 2) and single values with the element's own wire type, which each append one element.
- A varint is converted to the field type by keeping its low 32 or 64 bits as unsigned, then converting to the signed type for signed fields and enums, or zigzag decoding for `pb::zigzag` fields. `bool` is `true` when the varint is not 0.
- Records with a field number that `T` does not have are skipped: wire type 0 by reading a varint, 1 by skipping 8 bytes, 2 by reading a length and skipping that many bytes, 5 by skipping 4 bytes.
- The bytes are malformed, and `decode` returns `std::nullopt`, when a tag or varint runs past the end or is longer than 10 bytes, a length or fixed-size value runs past the end of the bytes, a field number is 0, a wire type is 3, 4, 6 or 7, a field of `T` comes with a wire type it does not accept, the bytes of a packed record do not end exactly at the end of a value, or a nested message is malformed.

Schema text:
- The first line is `syntax = "proto3";`. Then each definition follows, preceded by an empty line. Every line ends with `"\n"`.
- Definitions are written in this order: to write a message, first write, in field declaration order, the definition of each enum or message used by a field (as element type too) that is not written yet, then the message itself. `schema<T>()` writes `T` this way, so `T` comes last and every type is written once.
- An enum is `enum <Name> {`, a line `  <enumerator> = <value>;` per enumerator in declaration order, and `}`.
- A message is `message <Name> {`, a line `  <label><type> <member> = <number>;` per field in declaration order, and `}`. The label is `repeated ` for a `vector`, `optional ` for an `optional`, and empty otherwise. The type of a scalar is `bool`, `int32`, `int64`, `uint32`, `uint64`, `sint32`, `sint64` (with `pb::zigzag`), `float`, `double` or `string`; an enum or message is written as its unqualified name.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <cstdint>
#include <format>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

enum class Role { guest, member, admin = 7 };
enum Kind { home_office, branch = -1 };

struct Address {
    [[=pb::field(1)]] std::string city;
    [[=pb::field(2)]] std::uint32_t zip;
    [[=pb::field(3)]] Kind kind;
};

struct User {
    [[=pb::field(1)]] std::string name;
    [[=pb::field(2)]] std::int32_t id;
    [[=pb::field(3)]] [[=pb::zigzag]] std::int64_t balance;
    [[=pb::field(4)]] Role role;
    [[=pb::field(5)]] std::vector<std::int32_t> scores;
    [[=pb::field(6)]] std::vector<std::string> tags;
    [[=pb::field(7)]] Address home;
    [[=pb::field(8)]] std::vector<Address> offices;
    [[=pb::field(9)]] std::optional<std::int32_t> age;
    [[=pb::field(15)]] double ratio;
    [[=pb::field(16)]] float weight;
    [[=pb::field(20)]] [[=pb::zigzag]] std::vector<std::int32_t> deltas;
    int cache = 0;
};

struct Dup { [[=pb::field(1)]] int a; [[=pb::field(2)]] int b; [[=pb::field(1)]] int c; };
struct Zero { [[=pb::field(0)]] int a; };
struct Huge { [[=pb::field(536870911)]] int a; [[=pb::field(536870912)]] int b; };
struct Zig { [[=pb::field(1)]] [[=pb::zigzag]] std::uint32_t a; };
struct Odd { [[=pb::field(1)]] int a; [[=pb::field(2)]] char c; };
struct Grid { [[=pb::field(1)]] std::vector<std::vector<int>> rows; };

std::string hex(std::string_view bytes) {
    std::string out;
    for (char c : bytes) out += std::format("{:02x}", static_cast<unsigned char>(c));
    return out;
}

std::string bytes(std::string_view hex) {
    std::string out;
    for (std::size_t i = 0; i < hex.size(); i += 2) out += static_cast<char>(std::stoi(std::string(hex.substr(i, 2)), nullptr, 16));
    return out;
}

void show(std::string_view label, const std::optional<Address>& a) {
    if (!a) std::println("{}: malformed", label);
    else std::println("{}: city={} zip={} kind={}", label, a->city, a->zip, static_cast<int>(a->kind));
}

void show(const User& u) {
    std::print("name={} id={} balance={} role={} scores=[", u.name, u.id, u.balance, static_cast<int>(u.role));
    for (int s : u.scores) std::print(" {}", s);
    std::print(" ] tags=[");
    for (const std::string& t : u.tags) std::print(" {}", t);
    std::print(" ] home={}/{} offices=[", u.home.city, u.home.zip);
    for (const Address& a : u.offices) std::print(" {}/{}/{}", a.city, a.zip, static_cast<int>(a.kind));
    std::print(" ] age={} ratio={} weight={} deltas=[", u.age ? std::to_string(*u.age) : "none", u.ratio, u.weight);
    for (int d : u.deltas) std::print(" {}", d);
    std::println(" ] cache={}", u.cache);
}

int main() {
    std::print("{}", pb::schema<User>());
    std::println("---");
    std::println("User: {}", pb::check(^^User));
    std::println("{}", pb::check(^^Dup));
    std::println("{}", pb::check(^^Zero));
    std::println("{}", pb::check(^^Huge));
    std::println("{}", pb::check(^^Zig));
    std::println("{}", pb::check(^^Odd));
    std::println("{}", pb::check(^^Grid));
    std::println("---");

    std::println("address: {}", hex(pb::encode(Address{"Oslo", 150, branch})));
    std::println("empty user: {}", hex(pb::encode(User{})));
    User ada{.name = "ada", .id = -2, .balance = -3, .role = Role::admin, .scores = {1, 300}, .tags = {"x", ""},
             .home = {"Paris", 0, home_office}, .offices = {{}, {"Rome", 7, branch}}, .age = 0, .ratio = 0.5,
             .weight = 1.5f, .deltas = {-1, 1, -64}, .cache = 99};
    std::string wire = pb::encode(ada);
    std::println("ada: {} bytes", wire.size());
    std::println("{}", hex(wire));
    std::optional<User> back = pb::decode<User>(wire);
    std::print("decoded: ");
    show(*back);
    std::println("same bytes again: {}", pb::encode(*back) == wire);
    std::println("---");

    std::string mixed = bytes("a006010a03616e6e28052807"
                              "2a020102ad06010203043a060a04526f6d65"
                              "3a02102a0a02626fb20603787878b906010203040506070848002007");
    std::optional<User> m = pb::decode<User>(mixed);
    std::print("mixed: ");
    if (m) show(*m);
    else std::println("malformed");

    show("truncated", pb::decode<Address>(bytes("0a044f736c6f1096")));
    show("wrong wire type", pb::decode<Address>(bytes("1201ff")));
    show("group", pb::decode<Address>(bytes("1b")));
    show("field zero", pb::decode<Address>(bytes("0001")));
    show("length past end", pb::decode<Address>(bytes("0a054f736c6f")));
    show("long varint", pb::decode<Address>(bytes("10ffffffffffffffffffff01")));
    show("nothing", pb::decode<Address>(""));
    show("negative kind", pb::decode<Address>(bytes("18ffffffff0f")));
}
```

It must print exactly:

```
syntax = "proto3";

enum Role {
  guest = 0;
  member = 1;
  admin = 7;
}

enum Kind {
  home_office = 0;
  branch = -1;
}

message Address {
  string city = 1;
  uint32 zip = 2;
  Kind kind = 3;
}

message User {
  string name = 1;
  int32 id = 2;
  sint64 balance = 3;
  Role role = 4;
  repeated int32 scores = 5;
  repeated string tags = 6;
  Address home = 7;
  repeated Address offices = 8;
  optional int32 age = 9;
  double ratio = 15;
  float weight = 16;
  repeated sint32 deltas = 20;
}
---
User: ok
Dup.c: field number 1 is already used by a
Zero.a: field number 0 is out of range
Huge.b: field number 536870912 is out of range
Zig.a: zigzag needs a signed integer
Odd.c: unsupported type
Grid.rows: unsupported type
---
address: 0a044f736c6f10960118ffffffffffffffffff01
empty user: 3a00
ada: 85 bytes
0a0361646110feffffffffffffffff01180520072a0301ac0232017832003a070a055061726973420042130a04526f6d65100718ffffffffffffffffff01480079000000000000e03f85010000c03fa2010301027f
decoded: name=ada id=-2 balance=-3 role=7 scores=[ 1 300 ] tags=[ x  ] home=Paris/0 offices=[ /0/0 Rome/7/-1 ] age=0 ratio=0.5 weight=1.5 deltas=[ -1 1 -64 ] cache=0
same bytes again: true
---
mixed: name=bo id=0 balance=0 role=7 scores=[ 5 7 1 2 ] tags=[ ] home=Rome/42 offices=[ ] age=0 ratio=0 weight=0 deltas=[ ] cache=0
truncated: malformed
wrong wire type: malformed
group: malformed
field zero: malformed
length past end: malformed
long varint: malformed
nothing: city= zip=0 kind=0
negative kind: city= zip=0 kind=-1
```

Write the complete header to `solution.hpp` in the current directory.
