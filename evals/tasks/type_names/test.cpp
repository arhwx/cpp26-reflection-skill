#include "solution.hpp"

#include <print>
#include <string>

enum class Color { red, green };

struct Point {
    int x;
    int y;
};

struct Outer {
    struct Inner {};
};

template <class T>
struct Box {
    T value;
};

template <class A, class B>
struct Pair {
    A first;
    B second;
};

template <class... Ts>
struct Tuple {};

template <int N>
struct Tag {};

template <class T, int N>
struct Fixed {
    T data[N];
};

namespace geo {
struct Vec {};
template <class T>
struct Grid {};
}  // namespace geo

using IntBox = Box<int>;
using Count = unsigned;
using Coord = Pair<long, geo::Vec>;
template <class T>
using BoxOf = Box<T>;

template <class T>
void show() {
    std::string name = type_name<T>();
    std::println("{}", name);
}

int main() {
    show<int>();
    show<unsigned>();
    show<long>();
    show<bool>();
    show<char>();
    show<double>();
    show<Point>();
    show<Color>();
    show<Outer::Inner>();
    show<geo::Vec>();
    show<Box<Point>>();
    show<Pair<int, Box<double>>>();
    show<IntBox>();
    show<Count>();
    show<Box<Count>>();
    show<BoxOf<char>>();
    show<Coord>();
    show<Pair<Coord, Count>>();
    show<geo::Grid<IntBox>>();
    show<Tuple<>>();
    show<Tuple<char, Color, Tuple<bool>>>();
    show<Tag<-3>>();
    show<Fixed<Box<long>, 4>>();
    show<Box<Fixed<Outer::Inner, 2>>>();
}
