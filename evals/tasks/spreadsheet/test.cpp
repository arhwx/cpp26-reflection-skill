#include "solution.hpp"

#include <print>

struct Invoice {
    double price = 0;
    int qty = 0;
    double discount = 0;  // percent
    [[=calc::formula("price * qty")]] double gross = 0;
    [[=calc::formula("net * 0.25")]] double tax = 0;  // reads a member declared later
    [[=calc::formula("gross * discount / 100")]] double rebate = 0;
    [[=calc::formula("gross - rebate")]] double net = 0;
    [[=calc::formula("net + tax")]] double total = 0;
    [[=calc::formula("-(0 - total) / qty")]] double per_item = 0;
};

struct Grid {
    int w = 0;
    int h = 0;
    [[=calc::formula("w*h/2")]] int half = 0;  // computed as a double, then converted
    [[=calc::formula("half - (w - h) * 2.5")]] double mixed = 0;
};

struct Loop {
    [[=calc::formula("b + 1")]] double a;
    [[=calc::formula("c * 2")]] double b;
    [[=calc::formula("a")]] double c;
};

struct Typo {
    double width;
    [[=calc::formula("widht * 2")]] double twice;
};

struct Paren {
    double x;
    [[=calc::formula("(x + 1")]] double y;
};

constexpr Invoice sample = [] {
    Invoice i{.price = 20, .qty = 3, .discount = 10};
    return calc::recalc(i);
}();

int main() {
    Invoice inv{.price = 20, .qty = 3, .discount = 10};
    calc::recalc(inv);
    std::println("gross {} rebate {} net {} tax {} total {} per item {}", inv.gross, inv.rebate, inv.net, inv.tax, inv.total,
                 inv.per_item);
    std::println("{}", calc::check(^^Invoice));
    inv.qty = 10;
    std::println("with qty 10 the total is {}", calc::recalc(inv).total);
    std::println("at compile time: total {} per item {}", sample.total, sample.per_item);

    Grid g{.w = 3, .h = 5};
    calc::recalc(g);
    std::println("half {} mixed {}", g.half, g.mixed);
    std::println("{}", calc::check(^^Grid));

    std::println("{}", calc::check(^^Loop));
    std::println("{}", calc::check(^^Typo));
    std::println("{}", calc::check(^^Paren));
}
