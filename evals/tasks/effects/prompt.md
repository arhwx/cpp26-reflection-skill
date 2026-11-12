I want algebraic effects and handlers, as in Koka, OCaml 5 or Unison, on top of C++ coroutines. A program is a coroutine that asks for things by `co_await`-ing plain structs called effects. What a request means is decided by whoever runs the program, by passing handler objects. A handler is a plain struct, and its method for an effect is found by the type of the effect it takes, whatever the method is called. I also want a compile-time check that a set of handlers covers a set of effects, with a readable message.

An effect is an aggregate struct with public members only and a member type alias `result`, the type of the answer. `result` is either `void` or a type that `std::format("{}", ...)` accepts. Every data member of an effect is also accepted by `std::format("{}", ...)`.

Everything goes in `namespace eff`:
- `template <class R, class... Effects> struct eff::program`: the return type of a program coroutine. `R` is the value it `co_return`s and is never `void`. `Effects...` are the effect types it may ask for. Calling a program function only creates the program; it does not run any of its code until it is run by `run` or `trace` or awaited by another program.
- Inside a `program<R, Effects...>`:
  - `co_await e`, where `e` is an effect of a type `E` listed in `Effects...`, is a request. The program waits until a handler answers it, and the expression gives the answer as an `E::result` (nothing when it is `void`). A `co_await` on an effect type that is not listed must not compile.
  - `co_await sub()`, where `sub()` returns a `program<R2, Sub...>`, runs that program to its end and gives its `R2`. Its requests go to the same handlers and the same log as those of the outer program. If some type in `Sub...` is not listed in `Effects...`, it must not compile.
- `template <class R, class... Effects, class... Handlers> R eff::run(eff::program<R, Effects...> prog, Handlers&... handlers)`: runs `prog` to its end, answers each request with the handlers, and returns what `prog` returned.
- `template <class R, class... Effects, class... Handlers> R eff::trace(std::vector<std::string>* log, eff::program<R, Effects...> prog, Handlers&... handlers)`: the same as `run`, but when `log` is not null it appends one line to `*log` for each request, in the order the requests are answered.
- `consteval std::string_view eff::coverage(std::vector<std::meta::info> effects, std::vector<std::meta::info> handlers)`: tells whether the handler types cover the effect types (see below). `run` and `trace` must not compile when `coverage` for their `Effects...` and `Handlers...` is not `ok`.

Handlers:
- A method of handler type `H` for effect type `E` is a public member function of `H` that is not a constructor and has exactly one parameter, whose type is `E` after removing references and cv-qualifiers. Its name does not matter. Member function templates do not count.
- Each request goes to the first handler, in argument order, that has a method for its effect type. That method is called on the handler object that was passed in, with the effect object, and what it returns is the answer. Requests are answered one at a time, in the order the program makes them.

Log lines:
- An effect is written as its type name, `{`, then `member: value` for each data member in declaration order, separated by `, `, then `}`. Each value is formatted with `std::format("{}", value)`. An effect with no members is `Name{}`.
- For an effect whose `result` is `void`, the log line is just the written effect, like `Tell{text: hi}`. Otherwise it is the written effect, ` -> `, and the answer formatted with `{}`, like `Roll{sides: 6} -> 6`.

`coverage(effects, handlers)`:
- `handlers` is never empty. Type names in messages are the plain names of the types, and lists of names are separated by `, `.
- First, it goes through the handlers in order and, for each, through the effects in order. The first time a handler has more than one method for an effect, it returns `<Handler> has two handlers for <Effect>` (always the word `two`).
- Otherwise, if some effects have no handler with a method for them, it returns `no handler for ` followed by those effects in the order given, then ` among ` followed by all handler names in the order given.
- Otherwise it returns `ok`.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
? name
> rolled 4 and 5
> ada scores 9
result: ada:9:paid@1815
dice at 5, bank at 110
result: test:12:paid@0
  Ask{prompt: name} -> test
  Roll{sides: 6} -> 6
  Roll{sides: 6} -> 6
  Tell{text: rolled 6 and 6}
  Tell{text: test scores 12}
  Pay{to: test, amount: 120} -> true
  Now{} -> 0
? name
> rolled 6 and 6
> ada scores 12
result: ada:12:unpaid@0
  Ask{prompt: name} -> ada
  Roll{sides: 6} -> 6
  Roll{sides: 6} -> 6
  Tell{text: rolled 6 and 6}
  Tell{text: ada scores 12}
  Pay{to: ada, amount: 120} -> false
  Now{} -> 0
dice at 5, bank at 110
ok
no handler for Roll among Console, Clock
no handler for Ask, Roll, Pay among Clock
Loaded has two handlers for Roll
no handler for Roll among Picky, Console
```

Write the complete header to `solution.hpp` in the current directory.
