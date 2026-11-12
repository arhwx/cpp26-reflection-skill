I want C#-style change tracking for plain structs. `track::tracked<T>` holds a `T`, every write goes through a setter named by the member, `p.set<"price">(25)`, and the wrapper keeps one dirty flag per member, notifies observers and can commit or revert. The flags themselves should be a real generated struct with one `bool` per member, so code can read `p.dirty().price`.

Everything goes in `namespace track`. `T` is always an aggregate with only public non-static data members, no base classes, copyable, and every member type supports `==` and `std::format("{}")`. "Members" below means the non-static data members of `T` in declaration order.
- `track::untracked`: an annotation object for a member, `[[=track::untracked]] int views;`. Members without it are tracked. Other annotations are ignored.
- `track::flags<T>`: an aggregate struct with one non-static data member of type `bool` per tracked member of `T`, with the same name and in the same order, and nothing else. When `T` has no tracked members it is an empty class. Naming `flags<T>` again for the same `T` gives the same type.

`template <class T> class track::tracked`:
- `explicit tracked(T initial)`: holds `initial` as the current value. The committed value, used by `summary` and `revert`, starts as `initial` too. All flags are clear and there are no observers.
- `const T& value() const` and `const T* operator->() const`: read access to the current value.
- `get<"name">()`: a `const` member function template whose template argument is a string literal naming a member; returns a `const` reference to that member of the current value.
- `set<"name">(v)`: callable only when `T` has a member with that name and the type of `v` is implicitly convertible to the member's type; otherwise not callable, so a requires-expression that calls it is `false`. It converts `v` to the member's type. If the result is equal (`==`) to the member's current value, it returns `false` and nothing else happens. Otherwise it assigns the member and, if the member is tracked, sets its flag and then calls every observer, in the order they were added, with the member's name. It returns `true`. Untracked members are assigned the same way but have no flag and do not notify.
- `const flags<T>& dirty() const`: the flags.
- `is_dirty<"name">()`: a `const` member function template returning the flag of the named member as a `bool`. Callable only for tracked members.
- `std::vector<std::string_view> changed() const`: the names of the members whose flag is set, in declaration order.
- `std::string summary() const`: for each member whose flag is set, in declaration order, `<name>: <committed> -> <current>`, separated by `, `, with both values written as by `std::format("{}", value)`. When no flag is set, `no changes`.
- `void commit()`: the whole current value becomes the committed value, and all flags are cleared.
- `void revert()`: every member whose flag is set gets its committed value back, and all flags are cleared. Untracked members keep their current value. Observers are not called.
- `void on_change(f)`: adds an observer; `f` is a callable taking a `std::string_view`, such as a lambda that captures by reference.

A flag stays set until `commit` or `revert`, even when a later `set` restores the committed value.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

struct Product {
    std::string name;
    int price = 0;
    int stock = 0;
    [[=track::untracked]] int views = 0;
    std::vector<std::string> tags;
};

struct Counter {
    [[=track::untracked]] long hits = 0;
};

std::string join(const std::vector<std::string_view>& names) {
    std::string out = "[";
    for (std::string_view n : names) out += (out.size() > 1 ? " " : "") + std::string(n);
    return out + "]";
}

template <class T>
concept set_pricee = requires(T& t) { t.template set<"pricee">(1); };
template <class T>
concept set_price_text = requires(T& t) { t.template set<"price">("cheap"); };
template <class T>
concept set_price_double = requires(T& t) { t.template set<"price">(2.5); };
template <class T>
concept set_name_int = requires(T& t) { t.template set<"name">(42); };
template <class T>
concept dirty_views = requires(const T& t) { t.template is_dirty<"views">(); };
template <class T>
concept dirty_stock = requires(const T& t) { t.template is_dirty<"stock">(); };

int main() {
    track::tracked<Product> p(Product{"lamp", 20, 5, 0, {"home"}});
    int notified = 0;
    p.on_change([](std::string_view m) { std::println("  changed: {}", m); });
    p.on_change([&](std::string_view) { ++notified; });

    std::println("start: {} / {}", join(p.changed()), p.summary());
    std::println("set price 25: {}", p.set<"price">(25));
    std::println("set price 25 again: {}", p.set<"price">(25));
    std::println("set name: {}", p.set<"name">("desk lamp"));
    std::println("set views: {}", p.set<"views">(100));
    std::println("set stock 5: {}", p.set<"stock">(5));
    std::println("notified: {}", notified);
    std::println("changed: {}", join(p.changed()));

    auto [name, price, stock, tags] = p.dirty();
    std::println("flags: name={} price={} stock={} tags={}", name, price, stock, tags);
    std::println("dirty().price = {}, is_dirty<stock> = {}", p.dirty().price, p.is_dirty<"stock">());
    std::println("summary: {}", p.summary());
    std::println("price {} views {} name {}", p->price, p.get<"views">(), p.value().name);

    p.revert();
    std::println("after revert: {} {} {} views {}", p->name, p->price, join(p.changed()), p->views);
    std::println("summary: {}", p.summary());

    p.set<"tags">(std::vector<std::string>{"home", "sale"});
    p.set<"price">(18);
    std::println("summary: {}", p.summary());
    p.commit();
    std::println("after commit: {} / price {}", p.summary(), p->price);
    p.set<"price">(20);
    p.set<"price">(18);
    std::println("set back: {}", p.summary());
    p.revert();
    std::println("after revert: price {} tags {}", p->price, p->tags);
    std::println("notified: {}", notified);

    track::tracked<Counter> c(Counter{});
    c.on_change([](std::string_view m) { std::println("  counter changed: {}", m); });
    std::println("set hits: {}", c.set<"hits">(3));
    std::println("counter: hits {} changed {} summary {}", c->hits, join(c.changed()), c.summary());

    using F = track::flags<Product>;
    std::println("flags aggregate: {}, empty for Counter: {}", std::is_aggregate_v<F>,
                 std::is_empty_v<track::flags<Counter>>);
    std::println("flag types: {} {}", std::is_same_v<decltype(F{}.tags), bool>, sizeof(F) == 4 * sizeof(bool));
    std::println("same flags type: {}", std::is_same_v<std::remove_cvref_t<decltype(p.dirty())>, F>);

    using T = track::tracked<Product>;
    std::println("set<pricee>(1): {}", set_pricee<T>);
    std::println("set<price>(\"cheap\"): {}", set_price_text<T>);
    std::println("set<price>(2.5): {}", set_price_double<T>);
    std::println("set<name>(42): {}", set_name_int<T>);
    std::println("is_dirty<views>: {}", dirty_views<T>);
    std::println("is_dirty<stock>: {}", dirty_stock<T>);
}
```

It must print exactly:

```
start: [] / no changes
  changed: price
set price 25: true
set price 25 again: false
  changed: name
set name: true
set views: true
set stock 5: false
notified: 2
changed: [name price]
flags: name=true price=true stock=false tags=false
dirty().price = true, is_dirty<stock> = false
summary: name: lamp -> desk lamp, price: 20 -> 25
price 25 views 100 name desk lamp
after revert: lamp 20 [] views 100
summary: no changes
  changed: tags
  changed: price
summary: price: 20 -> 18, tags: ["home"] -> ["home", "sale"]
after commit: no changes / price 18
  changed: price
  changed: price
set back: price: 18 -> 18
after revert: price 18 tags ["home", "sale"]
notified: 6
set hits: true
counter: hits 3 changed [] summary no changes
flags aggregate: true, empty for Counter: true
flag types: true true
same flags type: true
set<pricee>(1): false
set<price>("cheap"): false
set<price>(2.5): true
set<name>(42): false
is_dirty<views>: false
is_dirty<stock>: true
```

Write the complete header to `solution.hpp` in the current directory.
