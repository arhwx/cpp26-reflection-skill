I want Rust's `#[derive(...)]` for C++ types. The derive list goes on the type as an annotation, `struct [[=rs::derive(rs::Debug, rs::Eq)]] Point`, and generic free functions work for exactly the types that derive the trait, with Rust's rule that a derive only holds when every field supports the trait too.

Everything goes in `namespace rs`.
- `rs::Debug`, `rs::Eq`, `rs::Ord`, `rs::Hash`: constants naming the four traits, usable as template arguments.
- `rs::derive(t...)`: an annotation value listing one or more of those traits, written as in the test. A trait may be listed twice. A type may carry several `rs::derive` annotations; their traits add up. Annotations of other types are ignored.
- A derived type is a class type, class template specialization or enumeration type that carries at least one `rs::derive` annotation. The test's classes are aggregates with only public members and no base classes; "members" below means their non-static data members in declaration order.

`rs::has<T, X>`, a concept whose second template argument is one of the trait constants (`rs::has<T, rs::Eq>`), tells whether `T` implements `X`:
- If `T` is not a derived type, it implements `X` exactly when C++ already supports it: `Debug` when `std::formattable<T, char>`, `Eq` when `std::equality_comparable<T>`, `Ord` when `std::totally_ordered<T>`, and `Hash` when `std::hash<T>{}(value)` is a valid expression for a `const T& value`.
- If `T` is a derived type, it implements `X` exactly when all of these hold: `X` is listed in its derive annotations; the type of every member (without cv-qualifiers) implements `X`, by these same rules; and for `Ord` and `Hash`, `T` also implements `Eq`. A derived enumeration has no members, so for it only the list and the `Eq` rule matter.

Functions, each callable only when `rs::has<T, X>` holds for its trait, so that a requires-expression calling it otherwise is `false`:
- `template <class T> std::string rs::debug(const T& value)` (`Debug`):
  - derived class: the type name, then ` { `, then `<member>: <debug of value>` for each member separated by `, `, then ` }`. With no members it is just the type name. The type name is the name as declared, without namespaces; for a class template specialization it is the template's name without arguments (`Pair`).
  - derived enumeration: the name of the enumerator equal to `value` (the test only uses values that are enumerators).
  - other types: `std::format("{:?}", value)` for `std::string`, `std::string_view` and `char`, and `std::format("{}", value)` for everything else.
- `template <class T> bool rs::eq(const T& a, const T& b)` (`Eq`): for a derived class, true when `rs::eq` holds for every member; otherwise `a == b`.
- `template <class T> std::weak_ordering rs::cmp(const T& a, const T& b)` (`Ord`): for a derived class, lexicographic: `rs::cmp` of the members in order, the first result that is not `equivalent` decides, and `equivalent` when there is none. Otherwise `less` when `a < b`, `greater` when `b < a`, and `equivalent` when neither.
- `template <class T> std::size_t rs::hash(const T& value)` (`Hash`): for a derived class, combines `rs::hash` of every member in an order-sensitive way, so that swapping two member values changes the hash in practice. Otherwise `std::hash<T>{}(value)`. Values that are equal by `rs::eq` must have equal hashes.

`template <class T> consteval std::string_view rs::check()` explains why a derive does not hold. For a derived type, for each trait it lists, in the fixed order `Debug`, `Eq`, `Ord`, `Hash`:
- first, the first member in declaration order whose type does not implement the trait gives `<type name>: member '<member>' does not implement <trait>`;
- then, for `Ord` and `Hash` when `Eq` is not listed, `<type name>: <trait> requires Eq`.

The first such message is returned. When there is none, or when `T` is not a derived type, the result is `ok`. Trait names are written `Debug`, `Eq`, `Ord` and `Hash`, and the type name follows the `rs::debug` rule.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <compare>
#include <print>
#include <string>
#include <unordered_set>
#include <vector>

namespace audit {
struct note {
    int level;
};
}  // namespace audit

struct [[=rs::derive(rs::Debug, rs::Eq, rs::Ord, rs::Hash)]] Point {
    int x;
    int y;
};

enum class [[=rs::derive(rs::Debug, rs::Eq, rs::Ord, rs::Hash)]] Color { Red, Green, Blue };
enum class Size { S, M, L };  // no derive: only what C++ gives it
enum class [[=rs::derive(rs::Debug)]] Mood { Calm, Angry };

struct [[=rs::derive(rs::Debug)]] [[=audit::note{1}]] [[=rs::derive(rs::Eq, rs::Hash, rs::Eq)]] Item {
    std::string name;
    Color color;
    Point at;
    double price;
    char grade;
};

template <class T>
struct [[=rs::derive(rs::Debug, rs::Eq, rs::Ord)]] Pair {
    T first;
    T second;
};

struct Plain {  // no derive and no operator==
    int v;
};

struct [[=rs::derive(rs::Debug, rs::Ord)]] Line {
    Point from;
    Point to;
};

struct [[=rs::derive(rs::Debug, rs::Eq)]] Cart {
    Size size;
    Plain extra;
};

struct [[=rs::derive(rs::Debug, rs::Eq, rs::Hash)]] Tagged {
    std::vector<std::string> tags;
    Mood mood;
};

struct [[=rs::derive(rs::Debug, rs::Eq)]] Unit {};

const char* text(std::weak_ordering o) {
    return o < 0 ? "less" : o > 0 ? "greater" : "equivalent";
}

template <class T>
concept comparable = requires(const T& a) { rs::cmp(a, a); };
template <class T>
concept printable = requires(const T& a) { rs::debug(a); };

int main() {
    Item lamp{"lamp", Color::Green, {1, 2}, 9.5, 'A'};
    std::println("{}", rs::debug(lamp));
    std::println("{}", rs::debug(Pair<Pair<int>>{{1, 2}, {3, 4}}));
    std::println("{}", rs::debug(Unit{}));
    std::println("{}", rs::debug(Tagged{{"new", "sale"}, Mood::Angry}));
    std::println("{} {} {}", rs::debug(std::string("hi \"you\"")), rs::debug(42), rs::debug('x'));

    Item copy = lamp;
    std::println("eq(lamp, copy) = {}", rs::eq(lamp, copy));
    copy.at.y = 3;
    std::println("eq(lamp, moved) = {}", rs::eq(lamp, copy));
    std::println("eq(Red, Red) = {}", rs::eq(Color::Red, Color::Red));

    std::println("cmp(Point{{1, 2}}, Point{{1, 3}}) = {}", text(rs::cmp(Point{1, 2}, Point{1, 3})));
    std::println("cmp(Point{{2, 0}}, Point{{1, 9}}) = {}", text(rs::cmp(Point{2, 0}, Point{1, 9})));
    std::println("cmp(Blue, Red) = {}", text(rs::cmp(Color::Blue, Color::Red)));
    std::println("cmp(L, S) = {}", text(rs::cmp(Size::L, Size::S)));
    std::println("cmp(pairs of points) = {}",
                 text(rs::cmp(Pair<Point>{{1, 1}, {5, 0}}, Pair<Point>{{1, 1}, {4, 9}})));
    std::println("cmp(pairs of doubles) = {}", text(rs::cmp(Pair<double>{0.5, 2}, Pair<double>{0.5, 2})));

    std::println("hash equal for equal items: {}", rs::hash(lamp) == rs::hash(Item(lamp)));
    std::println("hash depends on order: {}", rs::hash(Point{1, 2}) != rs::hash(Point{2, 1}));
    auto h = [](const Item& i) { return rs::hash(i); };
    auto e = [](const Item& a, const Item& b) { return rs::eq(a, b); };
    std::unordered_set<Item, decltype(h), decltype(e)> items(8, h, e);
    items.insert(lamp);
    items.insert(copy);
    items.insert(Item(lamp));
    std::println("distinct items: {}", items.size());

    std::println("has<Point, Hash>: {}", rs::has<Point, rs::Hash>);
    std::println("has<Item, Ord>: {}", rs::has<Item, rs::Ord>);
    std::println("has<Mood, Eq>: {}", rs::has<Mood, rs::Eq>);
    std::println("has<Size, Eq>: {}", rs::has<Size, rs::Eq>);
    std::println("has<Size, Debug>: {}", rs::has<Size, rs::Debug>);
    std::println("has<int, Ord>: {}", rs::has<int, rs::Ord>);
    std::println("has<Plain, Eq>: {}", rs::has<Plain, rs::Eq>);
    std::println("has<Pair<int>, Ord>: {}", rs::has<Pair<int>, rs::Ord>);
    std::println("has<Pair<Plain>, Eq>: {}", rs::has<Pair<Plain>, rs::Eq>);
    std::println("has<Pair<Mood>, Debug>: {}", rs::has<Pair<Mood>, rs::Debug>);
    std::println("has<Pair<Mood>, Ord>: {}", rs::has<Pair<Mood>, rs::Ord>);
    std::println("has<Line, Debug>: {}", rs::has<Line, rs::Debug>);
    std::println("has<Line, Ord>: {}", rs::has<Line, rs::Ord>);
    std::println("has<Tagged, Hash>: {}", rs::has<Tagged, rs::Hash>);
    std::println("comparable<Point>: {}, comparable<Item>: {}", comparable<Point>, comparable<Item>);
    std::println("printable<Cart>: {}, printable<Size>: {}", printable<Cart>, printable<Size>);

    std::println("check<Point>: {}", rs::check<Point>());
    std::println("check<Item>: {}", rs::check<Item>());
    std::println("check<Line>: {}", rs::check<Line>());
    std::println("check<Cart>: {}", rs::check<Cart>());
    std::println("check<Tagged>: {}", rs::check<Tagged>());
    std::println("check<Pair<Plain>>: {}", rs::check<Pair<Plain>>());
    std::println("check<Plain>: {}", rs::check<Plain>());
}
```

It must print exactly:

```
Item { name: "lamp", color: Green, at: Point { x: 1, y: 2 }, price: 9.5, grade: 'A' }
Pair { first: Pair { first: 1, second: 2 }, second: Pair { first: 3, second: 4 } }
Unit
Tagged { tags: ["new", "sale"], mood: Angry }
"hi \"you\"" 42 'x'
eq(lamp, copy) = true
eq(lamp, moved) = false
eq(Red, Red) = true
cmp(Point{1, 2}, Point{1, 3}) = less
cmp(Point{2, 0}, Point{1, 9}) = greater
cmp(Blue, Red) = greater
cmp(L, S) = greater
cmp(pairs of points) = greater
cmp(pairs of doubles) = equivalent
hash equal for equal items: true
hash depends on order: true
distinct items: 2
has<Point, Hash>: true
has<Item, Ord>: false
has<Mood, Eq>: false
has<Size, Eq>: true
has<Size, Debug>: false
has<int, Ord>: true
has<Plain, Eq>: false
has<Pair<int>, Ord>: true
has<Pair<Plain>, Eq>: false
has<Pair<Mood>, Debug>: true
has<Pair<Mood>, Ord>: false
has<Line, Debug>: true
has<Line, Ord>: false
has<Tagged, Hash>: false
comparable<Point>: true, comparable<Item>: false
printable<Cart>: false, printable<Size>: false
check<Point>: ok
check<Item>: ok
check<Line>: Line: Ord requires Eq
check<Cart>: Cart: member 'size' does not implement Debug
check<Tagged>: Tagged: member 'mood' does not implement Eq
check<Pair<Plain>>: Pair: member 'first' does not implement Debug
check<Plain>: ok
```

Write the complete header to `solution.hpp` in the current directory.
