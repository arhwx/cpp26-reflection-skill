I want Haskell's SmallCheck for plain structs: list every small value of a struct made of enums, bools, small integers and other such structs, then try a claim on all of them and print the first value that breaks it. I want this without writing a generator per type.

Everything goes in `namespace small`:
- `small::range{low, high}`: an annotation on an integer member, written `[[=small::range{0, 2}]]`. `low` and `high` are `int`s with `low <= high`.
- `template <class T> constexpr std::size_t small::count`: a variable template holding the number of values of `T`. It must be a constant expression.
- `small::values<T>()`: a function template that returns a range of every value of `T`, each once, in the order below. Iterating it gives the values as `T` objects or references to them. It can be a container such as `std::vector<T>` or a lazy range, but it must work in a range `for` loop and with `| std::views::take(n)` and `| std::views::drop(n)` as in the test.
- `template <class T> std::string small::show(const T& value)`: the value as text, in the format below.
- `template <class T> void small::check(std::string_view claim, auto property)`: calls `property(value)`, which returns `bool`, on the values of `T` in order.
  - At the first value for which it returns `false`, it prints `<claim>: broken by case <k>, <show(value)>` and a newline to standard output, where `<k>` is the position of that value in the order, counting from 1. It then stops; `property` is not called again.
  - If it returns `true` for every value, it prints `<claim>: holds in all <n> cases` and a newline, where `<n>` is the number of values.

Values of a type, in order:
- `bool`: `false`, then `true`.
- an enum: every enumerator, in declaration order.
- an integer type other than `bool`: every integer from `low` to `high` in ascending order, converted to that type. For a member with a `small::range{low, high}` annotation, those bounds are used. Otherwise the range is 0 to 2.
- a struct: every combination of the values of its members, where each member takes the values of its own type, with its own annotation if it is an integer. The combinations are in the order of nested loops over the members in declaration order: the first member changes slowest and the last member changes fastest. A member that is itself a struct takes the values of that struct, in that struct's order.

`count<T>` is the number of values in this order: 2 for `bool`, the number of enumerators for an enum, 3 for an integer type (the default range), and the product of the counts of the members for a struct, where an annotated integer member counts `high - low + 1`.

Text format of `show`:
- `bool`: `true` or `false`.
- an enum: the name of its enumerator.
- an integer: its decimal value, as by `std::format("{}")`.
- a struct: its name without namespaces, `{`, then `<member>: <show(member value)>` for each member in declaration order separated by `, `, then `}`.

Rules:
- Struct types in the test are aggregates whose members are all public non-static data members of the kinds above. Enums in the test have enumerators with distinct values.
- The annotation spelling must compile exactly as it appears in the test.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <ranges>

// a traffic light controller and the claim that it never shows green both ways
enum class Light { red, yellow, green };
struct Crossing {
    Light north, east;
    bool car_waiting;
    [[=small::range{0, 2}]] int timer;
};
bool safe(const Crossing& c) { return c.north == Light::red || c.east == Light::red; }

Crossing step(Crossing c, bool hasty) {
    if (c.timer > 0) return --c.timer, c;
    if (c.north == Light::green) {
        c.north = Light::yellow;
        if (hasty && c.car_waiting) c.east = Light::green;  // let the waiting car go a little early
    } else if (c.north == Light::yellow) c.north = Light::red, c.east = Light::green;
    else if (c.east == Light::green) c.east = Light::yellow;
    else if (c.east == Light::yellow) c.east = Light::red, c.north = Light::green;
    else if (c.car_waiting) c.east = Light::green;
    c.timer = 2;
    return c;
}

// nested structs, negative ranges and the default range
enum class Suit { clubs, hearts };
struct Card {
    Suit suit;
    [[=small::range{1, 3}]] int rank;
};
struct Hand {
    Card first, second;
    bool sorted;
};
struct Move {
    [[=small::range{-1, 1}]] short dx;
    [[=small::range{-1, 1}]] long dy;
    unsigned steps;
};

int main() {
    for (const Crossing& c : small::values<Crossing>() | std::views::take(4)) std::println("{}", small::show(c));
    constexpr std::size_t crossings = small::count<Crossing>;
    std::println("{} crossings", crossings);
    small::check<Crossing>("a safe crossing stays safe", [](const Crossing& c) { return !safe(c) || safe(step(c, false)); });
    small::check<Crossing>("the timer stays within 0 to 2", [](const Crossing& c) { return step(c, false).timer <= 2; });
    small::check<Crossing>("a hasty crossing stays safe", [](const Crossing& c) { return !safe(c) || safe(step(c, true)); });

    std::println("{} hands, {} cards, {} suits, {} bools", small::count<Hand>, small::count<Card>, small::count<Suit>,
                 small::count<bool>);
    for (int i = 0; const Card& c : small::values<Card>()) std::print("{}{}", i++ ? ", " : "", small::show(c));
    std::println();
    for (const Hand& h : small::values<Hand>() | std::views::drop(70)) std::println("{}", small::show(h));
    small::check<Hand>("a hand holds two different cards", [](const Hand& h) {
        return h.first.suit != h.second.suit || h.first.rank != h.second.rank;
    });
    small::check<Hand>("ranks add up to at most 5", [](const Hand& h) { return h.first.rank + h.second.rank <= 5; });
    small::check<Hand>("a sorted hand of one suit is ordered", [](const Hand& h) {
        return !h.sorted || h.first.suit != h.second.suit || h.first.rank <= h.second.rank;
    });
    small::check<Hand>("ranks stay within 1 to 3", [](const Hand& h) {
        return h.first.rank >= 1 && h.first.rank <= 3 && h.second.rank >= 1 && h.second.rank <= 3;
    });

    std::println("{} moves", small::count<Move>);
    for (const Move& m : small::values<Move>() | std::views::drop(24)) std::println("{}", small::show(m));
    small::check<Move>("a move goes somewhere", [](const Move& m) { return m.steps == 0 || m.dx != 0 || m.dy != 0; });
    small::check<Move>("a move has at most 2 steps", [](const Move& m) { return m.steps <= 2; });
    small::check<Light>("a light is never yellow", [](Light l) { return l != Light::yellow; });
    small::check<bool>("every bool is true", [](bool b) { return b; });
}
```

It must print exactly:

```
Crossing{north: red, east: red, car_waiting: false, timer: 0}
Crossing{north: red, east: red, car_waiting: false, timer: 1}
Crossing{north: red, east: red, car_waiting: false, timer: 2}
Crossing{north: red, east: red, car_waiting: true, timer: 0}
54 crossings
a safe crossing stays safe: holds in all 54 cases
the timer stays within 0 to 2: holds in all 54 cases
a hasty crossing stays safe: broken by case 40, Crossing{north: green, east: red, car_waiting: true, timer: 0}
72 hands, 6 cards, 2 suits, 2 bools
Card{suit: clubs, rank: 1}, Card{suit: clubs, rank: 2}, Card{suit: clubs, rank: 3}, Card{suit: hearts, rank: 1}, Card{suit: hearts, rank: 2}, Card{suit: hearts, rank: 3}
Hand{first: Card{suit: hearts, rank: 3}, second: Card{suit: hearts, rank: 3}, sorted: false}
Hand{first: Card{suit: hearts, rank: 3}, second: Card{suit: hearts, rank: 3}, sorted: true}
a hand holds two different cards: broken by case 1, Hand{first: Card{suit: clubs, rank: 1}, second: Card{suit: clubs, rank: 1}, sorted: false}
ranks add up to at most 5: broken by case 29, Hand{first: Card{suit: clubs, rank: 3}, second: Card{suit: clubs, rank: 3}, sorted: false}
a sorted hand of one suit is ordered: broken by case 14, Hand{first: Card{suit: clubs, rank: 2}, second: Card{suit: clubs, rank: 1}, sorted: true}
ranks stay within 1 to 3: holds in all 72 cases
27 moves
Move{dx: 1, dy: 1, steps: 0}
Move{dx: 1, dy: 1, steps: 1}
Move{dx: 1, dy: 1, steps: 2}
a move goes somewhere: broken by case 14, Move{dx: 0, dy: 0, steps: 1}
a move has at most 2 steps: holds in all 27 cases
a light is never yellow: broken by case 2, yellow
every bool is true: broken by case 1, false
```

Write the complete header to `solution.hpp` in the current directory.
