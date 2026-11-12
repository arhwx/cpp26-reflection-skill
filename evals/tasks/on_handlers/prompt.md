I want handlers found by naming convention: for an enum `E`, every enumerator `E::x` is handled by a function named `on_x` in a namespace of my choice. A dispatcher calls the right handler for a runtime value, and a forgotten or misspelled handler must be caught at compile time with a message that names it. Adding an enumerator and its handler must not require touching any table.

Everything goes in `namespace handlers`. `Ns` below is a reflection of a namespace (written `^^player` in the test) and `E` is an enumeration type, scoped or unscoped.

Definitions:
- The primary enumerators of `E` are its enumerators in declaration order, leaving out every enumerator whose value equals the value of an earlier enumerator (an alias, such as `halt = stop`).
- The handler of a primary enumerator `x` is a function declared directly in `Ns` named `on_x`, that is `on_` followed by the enumerator's name. Aliases have no handler of their own.
- The candidates of `Ns` are the functions declared directly in `Ns` whose name starts with `on_`. Variables, types, and anything in a nested or other namespace are never candidates. The test has no overloaded functions and no function templates in these namespaces.

API:
- `template <std::meta::info Ns, class E> consteval std::string_view handlers::check()`: describes what is wrong with the handlers of `E` in `Ns`.
  - The problems are, in this order: `missing on_x` for each primary enumerator `x` without a handler, in declaration order; then `unused on_y` for each candidate `on_y` that is not the handler of any primary enumerator, in ascending byte order of the names. A handler named after an alias is therefore unused.
  - The result is `ok` when there are no problems, otherwise the problems joined with `; `. The returned view refers to static storage.
- `template <std::meta::info Ns, class E, class... Args> R handlers::dispatch(E e, Args&&... args)`: called as `handlers::dispatch<^^player>(e, args...)`, with `E` deduced.
  - It takes part in overload resolution only when `check<Ns, E>()` is `ok`, so a call with a missing or unused handler fails in a way a requires-expression detects (`false`). The test checks this with a concept.
  - If `e` equals the value of a primary enumerator `x`, it calls `on_x` once with the arguments, perfectly forwarded, and returns its result. No other handler is called. For an alias value this is the handler of the primary enumerator with that value.
  - `R` is the return type of the handler of the first primary enumerator, and the result is converted to it. In the test, all handlers of an enum take the same parameters and return the same type, `int` or `void`.
  - If `e` is not the value of any enumerator, it calls nothing and throws `std::invalid_argument` whose `what()` is `no handler for value <v>`, where `<v>` is the underlying value of `e` written as a decimal integer (also when the underlying type is a character type such as `unsigned char`).

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <stdexcept>
#include <string>

enum class Cmd { start, stop, jump, halt = stop };

struct Player {
    int speed = 0;
    int height = 0;
};

namespace player {
int on_jump(Player& p, int h) {
    p.height = h;
    std::println("  jump {}", h);
    return 3;
}
int on_start(Player& p, int speed) {
    p.speed = speed;
    std::println("  start at {}", speed);
    return 1;
}
int on_stop(Player& p, int) {
    p.speed = 0;
    std::println("  stop");
    return 2;
}
// not handlers: a variable, a helper, and a function in a nested namespace
inline int on_count = 0;
inline int clamp(int x) { return x < 0 ? 0 : x; }
namespace detail {
inline int on_halt(Player&, int) { return 9; }
}  // namespace detail
}  // namespace player

namespace typo {
inline int on_start(Player&, int) { return 0; }
inline int on_stp(Player&, int) { return 0; }
inline int on_jump(Player&, int) { return 0; }
}  // namespace typo

namespace extra {
inline int on_zoom(Player&, int) { return 0; }
inline int on_start(Player&, int) { return 0; }
inline int on_stop(Player&, int) { return 0; }
inline int on_jump(Player&, int) { return 0; }
inline int on_halt(Player&, int) { return 0; }
inline int on_crouch(Player&, int) { return 0; }
}  // namespace extra

namespace nothing {}

enum Level : unsigned char { low = 1, high = 200 };

namespace levels {
std::string log;
void on_high(const char* who) { log += std::string(who) + " high;"; }
void on_low(const char* who) { log += std::string(who) + " low;"; }
}  // namespace levels

template <std::meta::info Ns, class E>
concept can_dispatch = requires(E e, Player p) { handlers::dispatch<Ns>(e, p, 1); };

int main() {
    Player p;
    std::println("start returns {}", handlers::dispatch<^^player>(Cmd::start, p, 5));
    std::println("jump returns {}", handlers::dispatch<^^player>(Cmd::jump, p, 2));
    std::println("player: speed {}, height {}", p.speed, p.height);
    std::println("halt returns {}", handlers::dispatch<^^player>(Cmd::halt, p, 0));
    std::println("player: speed {}, height {}", p.speed, p.height);
    try {
        handlers::dispatch<^^player>(static_cast<Cmd>(7), p, 0);
    } catch (const std::invalid_argument& e) {
        std::println("error: {}", e.what());
    }

    handlers::dispatch<^^levels>(high, "ada");
    handlers::dispatch<^^levels>(low, "bob");
    try {
        handlers::dispatch<^^levels>(static_cast<Level>(42), "cy");
    } catch (const std::invalid_argument& e) {
        std::println("error: {}", e.what());
    }
    std::println("levels log: {}", levels::log);

    std::println("player: {}", handlers::check<^^player, Cmd>());
    std::println("typo: {}", handlers::check<^^typo, Cmd>());
    std::println("extra: {}", handlers::check<^^extra, Cmd>());
    std::println("nothing: {}", handlers::check<^^nothing, Cmd>());
    std::println("levels: {}", handlers::check<^^levels, Level>());
    std::println("levels for Cmd: {}", handlers::check<^^levels, Cmd>());

    // an expression that contains ^^ must be constant evaluated, so the results are computed first
    constexpr bool player_ok = can_dispatch<^^player, Cmd>;
    constexpr bool typo_ok = can_dispatch<^^typo, Cmd>;
    constexpr bool extra_ok = can_dispatch<^^extra, Cmd>;
    std::println("dispatch player: {}", player_ok);
    std::println("dispatch typo: {}", typo_ok);
    std::println("dispatch extra: {}", extra_ok);
}
```

It must print exactly:

```
  start at 5
start returns 1
  jump 2
jump returns 3
player: speed 5, height 2
  stop
halt returns 2
player: speed 0, height 2
error: no handler for value 7
error: no handler for value 42
levels log: ada high;bob low;
player: ok
typo: missing on_stop; unused on_stp
extra: unused on_crouch; unused on_halt; unused on_zoom
nothing: missing on_start; missing on_stop; missing on_jump
levels: ok
levels for Cmd: missing on_start; missing on_stop; missing on_jump; unused on_high; unused on_low
dispatch player: true
dispatch typo: false
dispatch extra: false
```

Write the complete header to `solution.hpp` in the current directory.
