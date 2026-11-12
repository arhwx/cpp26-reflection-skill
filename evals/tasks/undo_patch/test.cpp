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
