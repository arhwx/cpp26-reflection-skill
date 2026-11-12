I want QuickCheck-style property testing for plain structs: generate random values of any struct from a seed, check a property on them, and when it fails, shrink the failing value to a small one. Members are described only by annotations, never by hand-written generators. Everything must be reproducible, so the generator and the shrinking steps are fixed exactly below.

API, all in `namespace fuzz`:
- `struct range { long long lo, hi; };` and `struct max_size { std::size_t n; };`: annotation types, used as `[[=fuzz::range{1, 9}]]` and `[[=fuzz::max_size{5}]]` on data members.
- `class rng`: `explicit rng(std::uint64_t seed)` and `std::uint64_t next()`.
- `template <class T> T arbitrary(rng& r)`: a random `T`.
- `template <class T> std::string show(const T& v)`: the text of a value.
- `template <class T> std::vector<T> shrinks(const T& v)`: the shrink candidates of `v`, in order.
- `template <class T, class P> void check(std::string_view name, std::uint64_t seed, int runs, P property)`: tests `property`, a callable taking `const T&` and returning `bool`.

Supported types: `bool`; integer types other than `bool` (the test uses `int`, `unsigned short` and `long long`); scoped enumerations; `std::string`; `std::vector<E>` of a supported `E`; and structs whose non-static data members are all public and of supported types. `T` in `arbitrary`, `shrinks` and `check` is always such a struct.

Options: every value is generated and shrunk with two options, a range `[lo, hi]` and a maximum length.
- For a data member they come from its annotations: `range` sets the range, `max_size` sets the maximum length. Without an annotation the range is `[0, 100]` and the maximum length is 3.
- An element of a vector member uses the vector member's range and the default maximum length 3.
- The top-level `T` uses the defaults.

`rng` is splitmix64. All arithmetic is on `std::uint64_t`, modulo 2^64. The state starts as `seed`. `next()` does: `state += 0x9e3779b97f4a7c15; z = state; z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9; z = (z ^ (z >> 27)) * 0x94d049bb133111eb; return z ^ (z >> 31);`.

Generating a value (each `next()` below is one call, made in the order written):
- `bool`: `next() % 2 == 1`.
- integer: `lo + next() % (hi - lo + 1)`, where `hi - lo + 1` is computed as `std::uint64_t` and the sum as `long long`, then converted to the integer type.
- enumeration: the enumerator at index `next() % count` in declaration order.
- `std::string`: a length `n = next() % (max + 1)`, then `n` characters, each `'a' + next() % 26`.
- `std::vector<E>`: a length `n = next() % (max + 1)`, then `n` elements generated in order.
- struct: each non-static data member in declaration order, depth first.

Shrink candidates of a value `v`, with its options. Two values are equal when all their members are equal, recursively. Every list leaves out candidates equal to `v` and candidates equal to an earlier candidate in the same list.
- `bool`: `false` if `v` is `true`, otherwise none.
- integer: let `t` be 0 clamped to `[lo, hi]`. None if `v == t`. Otherwise `t`, then `v - (v - t) / 2` (C++ division, rounding toward zero), then `v - 1` if `v > t` or `v + 1` if `v < t`.
- enumeration: every enumerator declared before the current one, in declaration order.
- `std::string`: for each position from first to last, the string with that character removed; then for each position from first to last whose character is not `'a'`, the string with that character replaced by `'a'`.
- `std::vector<E>`: for each position from first to last, the vector with that element removed; then for each position from first to last, for each candidate of that element in order, the vector with that element replaced by the candidate.
- struct: for each member in declaration order, for each candidate of that member in order, a copy of `v` with only that member replaced by the candidate.

`check` creates one `rng` from `seed` and generates up to `runs` values of `T` with it, one after another. For the first value `v` (test number `i`, counting from 1) where `property(v)` is false, it prints `<name>: falsified by test <i>: <show(v)>`. Then it shrinks: it goes through `shrinks(v)` in order, and at the first candidate for which `property` is false, it replaces `v` with it, counts one step and starts again from the candidates of the new `v`. When no candidate fails, it prints `<name>: shrunk in <steps> steps to <show(v)>` and returns. If all `runs` values pass, it prints `<name>: passed <runs> tests`. Each printed line ends with a newline.

`show`: `bool` as `true` or `false`; integers in decimal; enumerators by name; strings in double quotes with no escaping; vectors as `[a, b, c]` (`[]` when empty); structs as `<name>{<member>: <value>, <member>: <value>}` with the unqualified struct name and members in declaration order.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>
#include <vector>

enum class Kind { basic, express, bulk };

struct Item {
    [[=fuzz::range{1, 9}]] int qty;
    bool gift;
};

struct Order {
    [[=fuzz::range{100, 999}]] int id;
    Kind kind;
    [[=fuzz::max_size{5}]] std::string code;
    [[=fuzz::max_size{4}]] std::vector<Item> items;
    [[=fuzz::range{-20, 20}]] std::vector<int> adjust;
    unsigned short weight;
};

struct Window {
    [[=fuzz::range{-50, -10}]] long long low;
    [[=fuzz::range{5, 60}]] long long high;
};

int total_qty(const Order& o) {
    int n = 0;
    for (const Item& i : o.items) n += i.qty;
    return n;
}

int main() {
    fuzz::rng r(1);
    std::println("{}", r.next());
    for (int i = 0; i < 3; ++i) std::println("{}", fuzz::show(fuzz::arbitrary<Order>(r)));
    std::println("{}", fuzz::show(fuzz::arbitrary<Window>(r)));

    std::println("candidates:");
    for (const Item& c : fuzz::shrinks(Item{5, true})) std::println("  {}", fuzz::show(c));
    for (const Window& c : fuzz::shrinks(Window{-40, 11})) std::println("  {}", fuzz::show(c));
    Order o{101, Kind::bulk, "aab", {{2, false}}, {-1}, 0};
    for (const Order& c : fuzz::shrinks(o)) std::println("  {}", fuzz::show(c));

    fuzz::check<Order>("at most 3 items", 7, 200, [](const Order& o) { return o.items.size() <= 3; });
    fuzz::check<Order>("total quantity below 20", 7, 200, [](const Order& o) { return total_qty(o) < 20; });
    fuzz::check<Order>("no x in code", 7, 500, [](const Order& o) { return o.code.find('x') == std::string::npos; });
    fuzz::check<Order>("adjustments above -25", 3, 300, [](const Order& o) {
        int s = 0;
        for (int a : o.adjust) s += a;
        return s > -25;
    });
    fuzz::check<Order>("bulk orders weigh under 90", 11, 300, [](const Order& o) { return o.kind != Kind::bulk || o.weight < 90; });
    fuzz::check<Order>("id has three digits", 7, 300, [](const Order& o) { return o.id >= 100 && o.id <= 999; });
    fuzz::check<Window>("window wider than 25", 5, 100, [](const Window& w) { return w.high - w.low > 25; });
}
```

It must print exactly:

```
10451216379200822465
Order{id: 719, kind: basic, code: "fcrly", items: [], adjust: [1], weight: 86}
Order{id: 722, kind: express, code: "pbsqo", items: [Item{qty: 7, gift: false}, Item{qty: 1, gift: true}, Item{qty: 5, gift: true}, Item{qty: 2, gift: false}], adjust: [], weight: 100}
Order{id: 893, kind: bulk, code: "gxh", items: [Item{qty: 8, gift: false}, Item{qty: 4, gift: true}, Item{qty: 6, gift: true}], adjust: [], weight: 91}
Window{low: -35, high: 32}
candidates:
  Item{qty: 1, gift: true}
  Item{qty: 3, gift: true}
  Item{qty: 4, gift: true}
  Item{qty: 5, gift: false}
  Window{low: -10, high: 11}
  Window{low: -25, high: 11}
  Window{low: -39, high: 11}
  Window{low: -40, high: 5}
  Window{low: -40, high: 8}
  Window{low: -40, high: 10}
  Order{id: 100, kind: bulk, code: "aab", items: [Item{qty: 2, gift: false}], adjust: [-1], weight: 0}
  Order{id: 101, kind: basic, code: "aab", items: [Item{qty: 2, gift: false}], adjust: [-1], weight: 0}
  Order{id: 101, kind: express, code: "aab", items: [Item{qty: 2, gift: false}], adjust: [-1], weight: 0}
  Order{id: 101, kind: bulk, code: "ab", items: [Item{qty: 2, gift: false}], adjust: [-1], weight: 0}
  Order{id: 101, kind: bulk, code: "aa", items: [Item{qty: 2, gift: false}], adjust: [-1], weight: 0}
  Order{id: 101, kind: bulk, code: "aaa", items: [Item{qty: 2, gift: false}], adjust: [-1], weight: 0}
  Order{id: 101, kind: bulk, code: "aab", items: [], adjust: [-1], weight: 0}
  Order{id: 101, kind: bulk, code: "aab", items: [Item{qty: 1, gift: false}], adjust: [-1], weight: 0}
  Order{id: 101, kind: bulk, code: "aab", items: [Item{qty: 2, gift: false}], adjust: [], weight: 0}
  Order{id: 101, kind: bulk, code: "aab", items: [Item{qty: 2, gift: false}], adjust: [0], weight: 0}
at most 3 items: falsified by test 7: Order{id: 412, kind: bulk, code: "", items: [Item{qty: 2, gift: true}, Item{qty: 6, gift: false}, Item{qty: 3, gift: false}, Item{qty: 4, gift: true}], adjust: [10, 19], weight: 95}
at most 3 items: shrunk in 11 steps to Order{id: 100, kind: basic, code: "", items: [Item{qty: 1, gift: false}, Item{qty: 1, gift: false}, Item{qty: 1, gift: false}, Item{qty: 1, gift: false}], adjust: [], weight: 0}
total quantity below 20: falsified by test 1: Order{id: 787, kind: basic, code: "", items: [Item{qty: 8, gift: true}, Item{qty: 8, gift: false}, Item{qty: 9, gift: true}], adjust: [14, 18, -4], weight: 97}
total quantity below 20: shrunk in 9 steps to Order{id: 100, kind: basic, code: "", items: [Item{qty: 3, gift: false}, Item{qty: 8, gift: false}, Item{qty: 9, gift: false}], adjust: [], weight: 0}
no x in code: falsified by test 8: Order{id: 360, kind: express, code: "cxg", items: [Item{qty: 4, gift: false}, Item{qty: 2, gift: true}, Item{qty: 4, gift: false}], adjust: [-10], weight: 20}
no x in code: shrunk in 9 steps to Order{id: 100, kind: basic, code: "x", items: [], adjust: [], weight: 0}
adjustments above -25: falsified by test 6: Order{id: 801, kind: express, code: "ssjzv", items: [Item{qty: 6, gift: true}, Item{qty: 3, gift: true}], adjust: [-13, -13], weight: 58}
adjustments above -25: shrunk in 11 steps to Order{id: 100, kind: basic, code: "", items: [], adjust: [-12, -13], weight: 0}
bulk orders weigh under 90: falsified by test 46: Order{id: 390, kind: bulk, code: "cbdur", items: [Item{qty: 7, gift: true}, Item{qty: 9, gift: false}, Item{qty: 2, gift: true}, Item{qty: 1, gift: false}], adjust: [], weight: 94}
bulk orders weigh under 90: shrunk in 14 steps to Order{id: 100, kind: bulk, code: "", items: [], adjust: [], weight: 90}
id has three digits: passed 300 tests
window wider than 25: falsified by test 84: Window{low: -11, high: 13}
window wider than 25: shrunk in 2 steps to Window{low: -10, high: 5}
```

Write the complete header to `solution.hpp` in the current directory.
