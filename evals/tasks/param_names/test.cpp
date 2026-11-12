#include "solution.hpp"

#include <print>

int add(int lhs, int rhs);
void reset();
double scale(double value, int);
void log_line(const char* text, int level = 0);

namespace geo {
float distance(float x1, float y1, float x2, float y2);
}

struct Shape {
    void move(int dx, int dy);
    static Shape unit(double size);
};

int main() {
    std::println("{}", signature<^^add>());
    std::println("{}", signature<^^reset>());
    std::println("{}", signature<^^scale>());
    std::println("{}", signature<^^log_line>());
    std::println("{}", signature<^^geo::distance>());
    std::println("{}", signature<^^Shape::move>());
    std::println("{}", signature<^^Shape::unit>());
}
