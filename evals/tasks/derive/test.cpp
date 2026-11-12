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
