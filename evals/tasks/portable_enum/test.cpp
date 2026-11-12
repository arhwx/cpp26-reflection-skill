#include "solution.hpp"

#include <iostream>

static_assert(color_name(Color::green) == "green");
static_assert(color_name(static_cast<Color>(3)) == "unknown");

int main() {
    const Color colors[] = {Color::black, Color::red, Color::green, Color::blue, Color::white};
    for (Color c : colors)
        std::cout << static_cast<int>(c) << " -> " << color_name(c) << '\n';
    std::cout << "3 -> " << color_name(static_cast<Color>(3)) << '\n';
    std::cout << "255 -> " << color_name(static_cast<Color>(255)) << '\n';
}
