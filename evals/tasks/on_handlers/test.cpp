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
