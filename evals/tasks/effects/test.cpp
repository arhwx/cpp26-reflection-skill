#include "solution.hpp"

#include <format>
#include <print>
#include <string>
#include <vector>

// effects are plain structs that name their result type
struct Ask {
    std::string prompt;
    using result = std::string;
};
struct Tell {
    std::string text;
    using result = void;
};
struct Roll {
    int sides;
    using result = int;
};
struct Now {
    using result = int;
};
struct Pay {
    std::string to;
    int amount;
    using result = bool;
};

eff::program<int, Roll, Tell> roll_twice() {
    int a = co_await Roll{6};
    int b = co_await Roll{6};
    co_await Tell{std::format("rolled {} and {}", a, b)};
    co_return a + b;
}

eff::program<std::string, Ask, Tell, Roll, Now, Pay> game() {
    std::string name = co_await Ask{"name"};
    int score = co_await roll_twice();
    co_await Tell{std::format("{} scores {}", name, score)};
    bool paid = co_await Pay{name, score * 10};
    int now = co_await Now{};
    co_return std::format("{}:{}:{}@{}", name, score, paid ? "paid" : "unpaid", now);
}

// handlers are plain structs too. a method is found by the effect it takes, so its name is free
struct Console {
    std::string ask(const Ask& a) {
        std::println("? {}", a.prompt);
        return "ada";
    }
    void tell(const Tell& t) { std::println("> {}", t.text); }
};
struct Dice {
    int last = 3;
    int roll(const Roll& r) { return last = last % r.sides + 1; }
};
struct Clock {
    int now(Now) const { return 1815; }
};
struct Bank {
    int balance = 200;
    bool pay(const Pay& p) {
        if (p.amount > balance) return false;
        balance -= p.amount;
        return true;
    }
};
struct Script {  // a test double for everything
    std::string canned(const Ask&) { return "test"; }
    void ignore(const Tell&) {}
    int always_six(const Roll&) { return 6; }
    int frozen(const Now&) { return 0; }
    bool approve(const Pay&) { return true; }
};

struct Loaded {  // two methods for the same effect
    int low(const Roll&) { return 1; }
    int high(Roll) { return 6; }
};
struct Picky {  // nothing here counts as a handler for Roll
    Picky() = default;
    explicit Picky(const Roll& r) : seed(r.sides) {}
    int two(const Roll&, int bonus) { return seed + bonus; }
    int none() { return seed; }

private:
    int hidden(const Roll& r) { return r.sides; }
    int seed = 0;
};

int main() {
    Console console;
    Dice dice;
    Clock clock;
    Bank bank;
    std::println("result: {}", eff::run(game(), console, dice, clock, bank));
    std::println("dice at {}, bank at {}", dice.last, bank.balance);

    Script script;
    std::vector<std::string> log;
    std::println("result: {}", eff::trace(&log, game(), script));
    for (const std::string& line : log) std::println("  {}", line);

    std::vector<std::string> mixed;
    std::println("result: {}", eff::trace(&mixed, game(), console, bank, script, dice));
    for (const std::string& line : mixed) std::println("  {}", line);
    std::println("dice at {}, bank at {}", dice.last, bank.balance);

    std::println("{}", eff::coverage({^^Ask, ^^Tell, ^^Roll, ^^Now, ^^Pay}, {^^Script}));
    std::println("{}", eff::coverage({^^Ask, ^^Tell, ^^Roll, ^^Now}, {^^Console, ^^Clock}));
    std::println("{}", eff::coverage({^^Ask, ^^Roll, ^^Pay}, {^^Clock}));
    std::println("{}", eff::coverage({^^Roll}, {^^Dice, ^^Loaded}));
    std::println("{}", eff::coverage({^^Tell, ^^Roll}, {^^Picky, ^^Console}));
}
