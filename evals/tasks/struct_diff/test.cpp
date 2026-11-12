#include "solution.hpp"

#include <print>
#include <string>
#include <vector>

struct Point {
    int x;
    int y;
};

struct Pen {
    std::string color;
    double width;
};

struct Style {
    Pen pen;
    bool filled;
    float opacity;
};

struct Shape {
    const std::string id;
    Point origin;
    Point corners[3];
    int grid[2][2];
    std::string tags[2];
    Style style;
    static inline int created = 0;
    long version;
};

struct Team {
    std::string name;
    int scores[3];
};

struct League {
    Team teams[2];
    unsigned round;
};

void show(const std::vector<std::string>& lines) {
    if (lines.empty())
        std::println("no differences");
    for (const std::string& line : lines)
        std::println("{}", line);
    std::println("--");
}

int main() {
    const Shape a{"s1", {0, 0}, {{1, 2}, {3, 4}, {5, 6}}, {{1, 2}, {3, 4}}, {"red", "big"}, {{"black", 1.5}, true, 0.5f}, 1};
    show(diff(a, a));

    Shape b = a;
    b.origin.y = 5;
    b.corners[1].x = 30;
    b.corners[2].y = -6;
    b.grid[1][0] = 9;
    b.tags[1] = "small";
    b.style.pen.color = "blue";
    b.style.filled = false;
    b.style.opacity = 0.25f;
    b.version = 2;
    show(diff(a, b));

    const Shape c{"s2", {0, 0}, {{1, 2}, {3, 4}, {5, 6}}, {{1, 2}, {3, 4}}, {"red", "big"}, {{"black", 2.0}, true, 0.5f}, 1};
    show(diff(a, c));

    League before{{{"ants", {1, 2, 3}}, {"bees", {4, 5, 6}}}, 1};
    League after = before;
    after.teams[1].scores[2] = 7;
    after.teams[0].name = "wasps";
    after.round = 2;
    show(diff(before, after));
    show(diff(after, before));

    show(diff(Point{1, 2}, Point{-1, 2}));
}
