#include "solution.hpp"

#include <format>
#include <print>
#include <string>
#include <vector>

namespace game {

enum class Mode { windowed, fullscreen, borderless };

struct Pos {
    int x;
    int y;
};

inline int volume = 7;
inline double gravity = 9.81;
inline bool god_mode = false;
inline std::string player = "ann";
inline Mode mode = Mode::windowed;
inline constexpr int max_volume = 11;

// internal state, hidden from the console
inline Pos _pos{0, 0};
inline std::vector<std::string> _log;

std::string _where() { return std::format("at {},{}", _pos.x, _pos.y); }

int add(int a, int b) { return a + b; }

std::string greet(const std::string& name) { return "hello " + name; }

void spawn(const std::string& what, int count) { _log.push_back(std::format("{} x{}", what, count)); }

std::string move(int dx) {
    _pos.x += dx;
    return _where();
}

std::string move(int dx, int dy) {
    _pos.x += dx;
    _pos.y += dy;
    return _where();
}

bool louder(int by) {
    if (volume + by > max_volume)
        return false;
    volume += by;
    return true;
}

void reset() {
    volume = 7;
    god_mode = false;
}

double weight(double mass) { return mass * gravity; }

Mode next_mode(Mode m) { return m == Mode::borderless ? Mode::windowed : Mode(int(m) + 1); }

// templates are not commands
template <class T>
T twice(T value) {
    return value + value;
}

namespace detail {
inline int depth = 3;
}

}  // namespace game

int main() {
    const char* lines[] = {
        "volume",
        "volume 9",
        "volume loud",
        "volume 1 2",
        "max_volume",
        "max_volume 3",
        "gravity 1.5",
        "weight 2",
        "god_mode true",
        "god_mode yes",
        "player",
        "player \"ada lovelace\"",
        "mode",
        "mode fullscreen",
        "mode tiny",
        "next_mode borderless",
        "add 2 3",
        "  add   -4    1  ",
        "add 2",
        "add 2 x",
        "add x y",
        "greet bo",
        "greet \"big bo\"",
        "greet \"\"",
        "greet \"oops",
        "spawn orc 3",
        "spawn \"cave troll\" 1",
        "spawn orc",
        "move 2",
        "move 1 5",
        "move",
        "louder 1",
        "louder 5",
        "reset",
        "volume",
        "twice 2",
        "_pos",
        "_where",
        "detail",
        "depth",
        "Mode",
        "help",
        "",
    };
    for (const char* line : lines)
        std::println("[{}] [{}]", line, console::run<^^game>(line));

    std::println("log:");
    for (const std::string& entry : game::_log)
        std::println("  {}", entry);
    std::println("help:");
    std::println("{}", console::help<^^game>());
}
