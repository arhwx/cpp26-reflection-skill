Our editor keeps an undo history. Instead of copying the whole document for every edit, I want to store only what changed: `diff` of two versions gives a patch that records the old and new value of each changed member, and patches can be applied, inverted for undo, checked against a document and merged. The patch type must be generated from the struct, keeping its member names.

Everything goes in `namespace undo`. Below, "the members of `T`" means the non-static data members of `T`, in declaration order; static data members are ignored. All test structs are aggregates with only public members and no base classes.

Member kinds:
- A member whose type is a class type other than `std::string` or a `std::vector` specialization is a nested struct. Its members follow the same rules, recursively.
- Every other member is a leaf. The test's leaves are `int`, `bool`, `double`, `std::string` and `std::vector<std::string>`. Leaves are compared with `==` and are copied whole.

API:
- `template <class M> struct undo::change { M before; M after; };`: exactly these two members, in this order.
- `template <class T> using undo::patch = ...;`: an aggregate struct with one member per member of `T`, with the same name and in the same order. For a leaf of type `M` the patch member has type `std::optional<undo::change<M>>`. For a nested struct of type `S` it has type `undo::patch<S>`. A value-initialized patch (`patch<T>{}`) changes nothing. Naming `patch<T>` again for the same `T` gives the same type.
- `template <class T> patch<T> diff(const T& a, const T& b)`: for each leaf, recursively, the patch holds `change{a's value, b's value}` if the two values differ and nothing otherwise.
- `template <class T> void apply(T& object, const patch<T>& p)`: sets every leaf for which `p` holds a change to that change's `after`. All other leaves are left alone.
- `template <class T> bool can_apply(const T& object, const patch<T>& p)`: `true` when every leaf for which `p` holds a change currently equals that change's `before`. A patch with no changes can always be applied.
- `template <class P> P invert(const P& p)`: for a patch type `P`, the patch with the same changed leaves and `before` and `after` swapped. Applying `invert(p)` after `p` undoes `p`. `p` itself is not modified.
- `template <class P> P merge(const P& first, const P& second)`: one patch with the effect of applying `first` and then `second`. For each leaf: if only one of them holds a change, the result holds that change. If both do, the result holds `change{first's before, second's after}`, unless those two values are equal, in which case the result holds nothing for that leaf.
- `template <class P> std::size_t count(const P& p)`: the number of leaves, recursively, for which `p` holds a change.

`invert`, `merge` and `count` take the patch type itself as template parameter, so they work without naming the struct (`undo::invert(p)`).

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <type_traits>
#include <vector>

struct Pos {
    int line;
    int col;
};

struct Style {
    std::string font;
    int size;
    bool bold;
};

struct Doc {
    std::string title;
    std::vector<std::string> lines;
    Pos cursor;
    Style style;
    double zoom;
    static inline int opened = 0;
};

void show(const char* label, const Doc& d) {
    std::print("{}: {} [", label, d.title);
    for (std::size_t i = 0; i < d.lines.size(); ++i) std::print("{}{}", i ? "|" : "", d.lines[i]);
    std::println("] at {}:{}, {} {}{}, zoom {}", d.cursor.line, d.cursor.col, d.style.font, d.style.size,
                 d.style.bold ? " bold" : "", d.zoom);
}

int main() {
    using P = undo::patch<Doc>;
    std::println("title: {}", std::is_same_v<decltype(P::title), std::optional<undo::change<std::string>>>);
    std::println("lines: {}", std::is_same_v<decltype(P::lines), std::optional<undo::change<std::vector<std::string>>>>);
    std::println("cursor: {}", std::is_same_v<decltype(P::cursor), undo::patch<Pos>>);
    std::println("style.size: {}", std::is_same_v<decltype(P::style.size), std::optional<undo::change<int>>>);
    std::println("same type: {}", std::is_same_v<undo::patch<Doc>, P>);
    std::println("empty count: {}", undo::count(P{}));

    const Doc v0{"draft", {"hello"}, {1, 6}, {"mono", 12, false}, 1.0};
    Doc v1 = v0;
    v1.title = "notes";
    v1.lines.push_back("world");
    v1.cursor = {2, 1};
    Doc v2 = v1;
    v2.style.size = 14;
    v2.style.bold = true;
    v2.zoom = 1.5;
    v2.cursor.col = 3;
    Doc v3 = v2;
    v3.title = "draft";
    v3.zoom = 1.0;

    P p1 = undo::diff(v0, v1);
    P p2 = undo::diff(v1, v2);
    P p3 = undo::diff(v2, v3);
    std::println("p1: {} changes, title {} -> {}, {} -> {} lines", undo::count(p1), p1.title->before, p1.title->after,
                 p1.lines->before.size(), p1.lines->after.size());
    std::println("p1 cursor: line {} col {} zoom {}", p1.cursor.line.has_value(), p1.cursor.col.has_value(), p1.zoom.has_value());
    std::println("p2: {} changes, col {} -> {}, font changed {}", undo::count(p2), p2.cursor.col->before, p2.cursor.col->after,
                 p2.style.font.has_value());
    std::println("p3: {} changes", undo::count(p3));
    std::println("no change: {}", undo::count(undo::diff(v2, v2)));

    P back = undo::invert(p2);
    std::println("invert p2: {} changes, size {} -> {}", undo::count(back), back.style.size->before, back.style.size->after);
    std::println("p2 stays: size {} -> {}", p2.style.size->before, p2.style.size->after);

    std::println("can apply p2 to v1: {}", undo::can_apply(v1, p2));
    std::println("can apply p2 to v0: {}", undo::can_apply(v0, p2));
    std::println("can apply undo of p2 to v2: {}", undo::can_apply(v2, back));
    std::println("can apply empty patch: {}", undo::can_apply(v0, P{}));

    std::vector<P> history{p1, p2, p3};
    Doc doc = v3;
    show("now", doc);
    for (std::size_t i = history.size(); i-- > 0;) {
        undo::apply(doc, undo::invert(history[i]));
        show("undo", doc);
    }
    for (std::size_t i = 0; i < 2; ++i) {
        undo::apply(doc, history[i]);
        show("redo", doc);
    }

    P squash = undo::merge(p1, p2);
    std::println("p1+p2: {} changes, col {} -> {}", undo::count(squash), squash.cursor.col->before, squash.cursor.col->after);
    P all = undo::merge(squash, p3);
    std::println("p1+p2+p3: {} changes, title {}, zoom {}", undo::count(all), all.title.has_value(), all.zoom.has_value());
    std::println("p1 then its undo: {} changes", undo::count(undo::merge(p1, undo::invert(p1))));
    Doc fast = v0;
    undo::apply(fast, all);
    show("v0+all", fast);

    Doc other{"other", {"x"}, {9, 9}, {"serif", 10, false}, 2.0};
    undo::apply(other, p2);
    show("other+p2", other);
    std::println("opened: {}", Doc::opened);
}
```

It must print exactly:

```
title: true
lines: true
cursor: true
style.size: true
same type: true
empty count: 0
p1: 4 changes, title draft -> notes, 1 -> 2 lines
p1 cursor: line true col true zoom false
p2: 4 changes, col 1 -> 3, font changed false
p3: 2 changes
no change: 0
invert p2: 4 changes, size 14 -> 12
p2 stays: size 12 -> 14
can apply p2 to v1: true
can apply p2 to v0: false
can apply undo of p2 to v2: true
can apply empty patch: true
now: draft [hello|world] at 2:3, mono 14 bold, zoom 1
undo: notes [hello|world] at 2:3, mono 14 bold, zoom 1.5
undo: notes [hello|world] at 2:1, mono 12, zoom 1
undo: draft [hello] at 1:6, mono 12, zoom 1
redo: notes [hello|world] at 2:1, mono 12, zoom 1
redo: notes [hello|world] at 2:3, mono 14 bold, zoom 1.5
p1+p2: 7 changes, col 6 -> 3
p1+p2+p3: 5 changes, title false, zoom false
p1 then its undo: 0 changes
v0+all: draft [hello|world] at 2:3, mono 14 bold, zoom 1
other+p2: other [x] at 9:3, serif 14 bold, zoom 1.5
opened: 0
```

Write the complete header to `solution.hpp` in the current directory.
