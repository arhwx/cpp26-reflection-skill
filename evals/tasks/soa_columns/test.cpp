#include "solution.hpp"

#include <print>
#include <string>
#include <type_traits>
#include <vector>

struct Point {
    int x;
    double y;
};

struct Particle {
    float mass;
    std::string label;
    int id;
};

static_assert(std::is_same_v<decltype(soa_columns<Point>::x), std::vector<int>>);
static_assert(std::is_same_v<decltype(soa_columns<Point>::y), std::vector<double>>);
static_assert(std::is_same_v<decltype(soa_columns<Particle>::label), std::vector<std::string>>);

int main() {
    soa_columns<Point> c;
    c.x.push_back(1);
    c.y.push_back(2.5);
    push(c, Point{3, 4.5});
    push(c, Point{5, 6.5});
    std::println("x: {}", c.x);
    std::println("y: {}", c.y);

    soa_columns<Particle> p;
    push(p, Particle{1.5f, "a", 7});
    push(p, Particle{0.25f, "b", 8});
    std::println("mass: {}", p.mass);
    std::println("label: {}", p.label);
    std::println("id: {}", p.id);
}
