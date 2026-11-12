#include "solution.hpp"

#include <print>
#include <string>
#include <vector>

enum class Kind { basic, express, bulk };

struct Item {
    [[=fuzz::range{1, 9}]] int qty;
    bool gift;
};

struct Order {
    [[=fuzz::range{100, 999}]] int id;
    Kind kind;
    [[=fuzz::max_size{5}]] std::string code;
    [[=fuzz::max_size{4}]] std::vector<Item> items;
    [[=fuzz::range{-20, 20}]] std::vector<int> adjust;
    unsigned short weight;
};

struct Window {
    [[=fuzz::range{-50, -10}]] long long low;
    [[=fuzz::range{5, 60}]] long long high;
};

int total_qty(const Order& o) {
    int n = 0;
    for (const Item& i : o.items) n += i.qty;
    return n;
}

int main() {
    fuzz::rng r(1);
    std::println("{}", r.next());
    for (int i = 0; i < 3; ++i) std::println("{}", fuzz::show(fuzz::arbitrary<Order>(r)));
    std::println("{}", fuzz::show(fuzz::arbitrary<Window>(r)));

    std::println("candidates:");
    for (const Item& c : fuzz::shrinks(Item{5, true})) std::println("  {}", fuzz::show(c));
    for (const Window& c : fuzz::shrinks(Window{-40, 11})) std::println("  {}", fuzz::show(c));
    Order o{101, Kind::bulk, "aab", {{2, false}}, {-1}, 0};
    for (const Order& c : fuzz::shrinks(o)) std::println("  {}", fuzz::show(c));

    fuzz::check<Order>("at most 3 items", 7, 200, [](const Order& o) { return o.items.size() <= 3; });
    fuzz::check<Order>("total quantity below 20", 7, 200, [](const Order& o) { return total_qty(o) < 20; });
    fuzz::check<Order>("no x in code", 7, 500, [](const Order& o) { return o.code.find('x') == std::string::npos; });
    fuzz::check<Order>("adjustments above -25", 3, 300, [](const Order& o) {
        int s = 0;
        for (int a : o.adjust) s += a;
        return s > -25;
    });
    fuzz::check<Order>("bulk orders weigh under 90", 11, 300, [](const Order& o) { return o.kind != Kind::bulk || o.weight < 90; });
    fuzz::check<Order>("id has three digits", 7, 300, [](const Order& o) { return o.id >= 100 && o.id <= 999; });
    fuzz::check<Window>("window wider than 25", 5, 100, [](const Window& w) { return w.high - w.low > 25; });
}
