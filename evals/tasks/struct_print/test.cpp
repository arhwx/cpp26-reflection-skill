#include "solution.hpp"

#include <print>
#include <string>

struct Inner {
    int a;
};

struct Point {
    int x;
    double y;
    std::string name;
    Inner inner;
};

struct Empty {};

struct Deep {
    Point p;
    Empty e;
    double ratio;
};

int main() {
    std::println("{}", to_string(Point{1, 2.5, "ab", {3}}));
    std::println("{}", to_string(Inner{-7}));
    std::println("{}", to_string(Empty{}));
    std::println("{}", to_string(Deep{{0, 0.1, "", {42}}, {}, 3.0}));

    Point p{10, -0.5, "long name with spaces", {0}};
    p.inner.a = 99;
    std::println("{}", to_string(p));
}
