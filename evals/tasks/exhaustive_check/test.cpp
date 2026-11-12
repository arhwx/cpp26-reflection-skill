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
