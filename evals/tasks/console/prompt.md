I want a game-style console for a namespace, like Python Fire or Quake cvars: the namespace's variables become settings and its functions become commands, typed as one line of text. Nothing is registered by hand.

API, in namespace `console`:
- `template <std::meta::info Ns> std::string run(std::string_view line)`: runs one command line against the namespace `Ns` (the test passes `^^game`) and returns the one-line reply.
- `template <std::meta::info Ns> std::string help()`: lists the entries of `Ns`, one per line, joined with `'\n'` and with no trailing newline.

Entries:
- The entries are the variables and functions declared directly in `Ns` whose names don't start with `_`. Types, templates and nested namespaces are not entries, and neither is anything inside a nested namespace. A name that is not an entry is unknown, even if it exists. The types of hidden entries may be unsupported, so they must not be looked at.
- Variable, parameter and return types are `int`, `double`, `bool`, `std::string` or an enum type. Parameters are taken by value or by const reference, and a function may also return `void`. Functions have no default arguments, and overloads of one name have different parameter counts.

Splitting a line into tokens:
- Tokens are separated by one or more spaces. Leading and trailing spaces are ignored.
- A token that starts with `"` runs to the next `"`. The text between the quotes is the token, and it may contain spaces or be empty. After the closing quote the next token starts.
- If a quote is not closed, the reply is `error: unterminated quote`. A line with no tokens gives an empty reply.

Reading a token as a value of a type:
- `int` and `double`: `std::from_chars` (default format) must read the whole token.
- `bool`: only `true` or `false`.
- `std::string`: the token as is.
- An enum: the name of one of its enumerators.

Writing a value: `int`, `double` and `bool` as `std::format("{}", v)` does; a `std::string` inside double quotes, with no escaping; an enum as the name of its enumerator.

Replies, where `<name>` is the first token:
- `<name>` is not an entry: `error: unknown command '<name>'`.
- `<name>` is a variable and is the only token: `<name> = <value>`.
- `<name>` is a variable followed by one token: if the variable is const, `error: <name> is read-only`; otherwise if the token can't be read as the variable's type, `error: bad value '<token>' for <name>`; otherwise the variable is assigned and the reply is `<name> = <new value>`.
- `<name>` is a variable followed by more than one token: `error: <name> takes one value`.
- `<name>` is a function: the remaining tokens are the arguments, and the overload with that many parameters is called. If there is none, `error: <name> does not take <count> arguments`, with the count in decimal. Arguments are read left to right; at the first one that can't be read as its parameter's type the reply is `error: bad argument '<token>' for <parameter name>` and nothing is called. The reply to a call is the written return value, or `ok` for a `void` function.

`help` lines, sorted by name (plain `char` comparison), and overloads of one name by parameter count:
- a variable: `<name>: <type> = <value>`, followed by ` (read-only)` if it is const.
- a function: `<name>(<type> <parameter>, <type> <parameter>) -> <return type>`, with `()` for no parameters.
- Type names are `int`, `double`, `bool`, `string`, `void`, and an enum's own unqualified name. A parameter's type is named without its const and reference.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
[volume] [volume = 7]
[volume 9] [volume = 9]
[volume loud] [error: bad value 'loud' for volume]
[volume 1 2] [error: volume takes one value]
[max_volume] [max_volume = 11]
[max_volume 3] [error: max_volume is read-only]
[gravity 1.5] [gravity = 1.5]
[weight 2] [3]
[god_mode true] [god_mode = true]
[god_mode yes] [error: bad value 'yes' for god_mode]
[player] [player = "ann"]
[player "ada lovelace"] [player = "ada lovelace"]
[mode] [mode = windowed]
[mode fullscreen] [mode = fullscreen]
[mode tiny] [error: bad value 'tiny' for mode]
[next_mode borderless] [windowed]
[add 2 3] [5]
[  add   -4    1  ] [-3]
[add 2] [error: add does not take 1 arguments]
[add 2 x] [error: bad argument 'x' for b]
[add x y] [error: bad argument 'x' for a]
[greet bo] ["hello bo"]
[greet "big bo"] ["hello big bo"]
[greet ""] ["hello "]
[greet "oops] [error: unterminated quote]
[spawn orc 3] [ok]
[spawn "cave troll" 1] [ok]
[spawn orc] [error: spawn does not take 1 arguments]
[move 2] ["at 2,0"]
[move 1 5] ["at 3,5"]
[move] [error: move does not take 0 arguments]
[louder 1] [true]
[louder 5] [false]
[reset] [ok]
[volume] [volume = 7]
[twice 2] [error: unknown command 'twice']
[_pos] [error: unknown command '_pos']
[_where] [error: unknown command '_where']
[detail] [error: unknown command 'detail']
[depth] [error: unknown command 'depth']
[Mode] [error: unknown command 'Mode']
[help] [error: unknown command 'help']
[] []
log:
  orc x3
  cave troll x1
help:
add(int a, int b) -> int
god_mode: bool = false
gravity: double = 1.5
greet(string name) -> string
louder(int by) -> bool
max_volume: int = 11 (read-only)
mode: Mode = fullscreen
move(int dx) -> string
move(int dx, int dy) -> string
next_mode(Mode m) -> Mode
player: string = "ada lovelace"
reset() -> void
spawn(string what, int count) -> void
volume: int = 7
weight(double mass) -> double
```

Write the complete header to `solution.hpp` in the current directory.
