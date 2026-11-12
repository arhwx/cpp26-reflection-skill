Our tools have a settings panel that someone has to update by hand whenever a settings struct changes. I want one generic inspector that draws any struct as a form and edits a field by its path, with valid ranges and display labels written as annotations on the members and enumerators offered as choices. For now the form is plain text so it can be tested.

Everything goes in `namespace inspect`:
- `inspect::range(lo, hi)`: annotation giving the inclusive valid range of a numeric member. `lo` and `hi` are stored as `double`; the test writes them as integer literals.
- `inspect::label("text")`: annotation giving the text shown for a member instead of its name.
- `inspect::readonly`: annotation that makes a member read-only.
- `template <class T> std::string inspect::render(const T& obj)`: the form as text.
- `template <class T> std::string inspect::set(T& obj, std::string_view path, std::string_view text)`: sets one field from text and returns a result message.

Members:
- Only non-static data members are shown, in declaration order. `T` and every nested struct have only public members.
- A member is a group if its type is a class type other than `std::string`. Every other member is a field, and its type is `bool`, `std::string`, an enumeration, or an integer or floating-point type.
- A field is read-only if it carries `inspect::readonly` or lies anywhere inside a group member that carries it.
- Members may also carry annotations from other libraries, like `audit::note` in the test. Those must be ignored.

`render`:
- The first line is the unqualified name of `T`. Each member follows on its own line, indented by two spaces per nesting level, so the members of `T` get two spaces and the members of a group get two more than the group. Every line, including the last, ends with `\n`.
- The text shown for a member is its `inspect::label` text if it has one, otherwise its name.
- A group is a line `<text>:` followed by its members.
- A field is a line `<text> = <value>`, where `<value>` is:
  - `bool`: `[x]` for true, `[ ]` for false.
  - `std::string`: the string in double quotes, with no escaping.
  - enumeration: every enumerator of the type in declaration order, separated by single spaces, each written as `[name]` if its value equals the field and as `(name)` otherwise. Several enumerators can be in brackets when they share a value, and none when the value matches no enumerator.
  - number: the value formatted like `std::format("{}", value)`. If the field has `inspect::range`, ` (<lo>..<hi>)` follows, with both bounds formatted like `std::format("{}", bound)` for a `double`. If the value is below `lo` or above `hi`, ` out of range` follows that.
- A read-only field ends with ` (read-only)`.

`set`:
- `path` is a list of member names separated by `.`, starting in `T`: each name before the last selects a group member, and the last selects a field. Paths use member names, never labels.
- On success it changes the field and returns `ok`. On failure it leaves `obj` unchanged and returns `<path>: <problem>`, with the problem chosen by the first rule that applies while walking the path from the left:
  - a name is not a member of the struct it is looked up in, or a field is followed by more names: `no such field`
  - the path ends at a group: `is a group`
  - the field is read-only: `read-only`
  - otherwise the text is converted, and fails as below.
- Conversions:
  - `bool`: exactly `true` or `false`, else `"<text>" is not true or false`.
  - `std::string`: any text, taken as is.
  - enumeration: the name of an enumerator of the type, matched exactly, else `"<text>" is not one of <names>`, where `<names>` lists every enumerator name in declaration order separated by `, `. When two enumerators share a value, the field gets that value either way.
  - integer: the whole text must be read by `std::from_chars` in base 10, else `"<text>" is not a whole number`. So the empty text, `7.5` and `+5` fail, and `010` is 10.
  - floating point: the whole text must be read by `std::from_chars` with its default format, else `"<text>" is not a number`.
  - after a number is read, if the field has `inspect::range` and the number is below `lo` or above `hi`: `<number> is outside <lo>..<hi>`, with the number and the bounds formatted as in `render`.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>

namespace audit {
struct note {
    int level;
};
}  // namespace audit

enum class Mode { light, dark, system };
enum Quality { low, medium, high, best = high };

struct Display {
    [[=inspect::range(640, 7680)]] int width = 1920;
    [[=inspect::range(480, 4320)]] int height = 1080;
    Quality quality = high;
    [[=inspect::label("refresh rate")]] [[=inspect::range(30, 240)]] double hz = 59.94;
};

struct Info {
    std::string build = "2026.11";
    int plugins = 3;
    bool licensed = true;
};

struct Studio {
    [[=inspect::label("Studio name")]] std::string name = "main room";
    [[=inspect::range(0, 10)]] int volume = 7;
    [[=inspect::range(0, 1)]] double gain = 0.25;
    Mode mode = Mode::dark;
    bool fullscreen = true;
    [[=inspect::readonly]] int id = 42;
    [[=audit::note{3}]] bool muted = false;
    [[=inspect::label("Screen")]] Display display;
    [[=inspect::readonly]] Info info;
    long frames = -12;
};

void edit(Studio& s, std::string_view path, std::string_view text) {
    std::println("set {} to \"{}\": {}", path, text, inspect::set(s, path, text));
}

int main() {
    Studio s;
    std::print("{}", inspect::render(s));

    edit(s, "volume", "9");
    edit(s, "volume", "12");
    edit(s, "volume", "-1");
    edit(s, "volume", "7.5");
    edit(s, "volume", "");
    edit(s, "volume", "010");
    edit(s, "gain", "1.5");
    edit(s, "gain", "half");
    edit(s, "gain", "0.75");
    edit(s, "mode", "System");
    edit(s, "mode", "system");
    edit(s, "fullscreen", "yes");
    edit(s, "fullscreen", "false");
    edit(s, "muted", "true");
    edit(s, "id", "1");
    edit(s, "name", "B studio");
    edit(s, "Studio name", "C");
    edit(s, "display", "1");
    edit(s, "display.width", "3840");
    edit(s, "display.height", "100");
    edit(s, "display.depth", "1");
    edit(s, "display.quality", "low");
    edit(s, "display.quality", "ultra");
    edit(s, "display.hz", "143.5");
    edit(s, "info.plugins", "4");
    edit(s, "info", "x");
    edit(s, "volume.max", "3");
    edit(s, "frames", "99999");
    edit(s, "frames", "+5");
    edit(s, "nothing", "1");
    std::print("{}", inspect::render(s));

    std::println("-- best is high");
    edit(s, "display.quality", "best");
    s.volume = 15;
    s.gain = -0.5;
    s.mode = static_cast<Mode>(7);
    std::print("{}", inspect::render(s));

    std::println("-- a display alone");
    Display d;
    d.hz = 240;
    d.width = 639;
    std::print("{}", inspect::render(d));
}
```

It must print exactly:

```
Studio
  Studio name = "main room"
  volume = 7 (0..10)
  gain = 0.25 (0..1)
  mode = (light) [dark] (system)
  fullscreen = [x]
  id = 42 (read-only)
  muted = [ ]
  Screen:
    width = 1920 (640..7680)
    height = 1080 (480..4320)
    quality = (low) (medium) [high] [best]
    refresh rate = 59.94 (30..240)
  info:
    build = "2026.11" (read-only)
    plugins = 3 (read-only)
    licensed = [x] (read-only)
  frames = -12
set volume to "9": ok
set volume to "12": volume: 12 is outside 0..10
set volume to "-1": volume: -1 is outside 0..10
set volume to "7.5": volume: "7.5" is not a whole number
set volume to "": volume: "" is not a whole number
set volume to "010": ok
set gain to "1.5": gain: 1.5 is outside 0..1
set gain to "half": gain: "half" is not a number
set gain to "0.75": ok
set mode to "System": mode: "System" is not one of light, dark, system
set mode to "system": ok
set fullscreen to "yes": fullscreen: "yes" is not true or false
set fullscreen to "false": ok
set muted to "true": ok
set id to "1": id: read-only
set name to "B studio": ok
set Studio name to "C": Studio name: no such field
set display to "1": display: is a group
set display.width to "3840": ok
set display.height to "100": display.height: 100 is outside 480..4320
set display.depth to "1": display.depth: no such field
set display.quality to "low": ok
set display.quality to "ultra": display.quality: "ultra" is not one of low, medium, high, best
set display.hz to "143.5": ok
set info.plugins to "4": info.plugins: read-only
set info to "x": info: is a group
set volume.max to "3": volume.max: no such field
set frames to "99999": ok
set frames to "+5": frames: "+5" is not a whole number
set nothing to "1": nothing: no such field
Studio
  Studio name = "B studio"
  volume = 10 (0..10)
  gain = 0.75 (0..1)
  mode = (light) (dark) [system]
  fullscreen = [ ]
  id = 42 (read-only)
  muted = [x]
  Screen:
    width = 3840 (640..7680)
    height = 1080 (480..4320)
    quality = [low] (medium) (high) (best)
    refresh rate = 143.5 (30..240)
  info:
    build = "2026.11" (read-only)
    plugins = 3 (read-only)
    licensed = [x] (read-only)
  frames = 99999
-- best is high
set display.quality to "best": ok
Studio
  Studio name = "B studio"
  volume = 15 (0..10) out of range
  gain = -0.5 (0..1) out of range
  mode = (light) (dark) (system)
  fullscreen = [ ]
  id = 42 (read-only)
  muted = [x]
  Screen:
    width = 3840 (640..7680)
    height = 1080 (480..4320)
    quality = (low) (medium) [high] [best]
    refresh rate = 143.5 (30..240)
  info:
    build = "2026.11" (read-only)
    plugins = 3 (read-only)
    licensed = [x] (read-only)
  frames = 99999
-- a display alone
Display
  width = 639 (640..7680) out of range
  height = 1080 (480..4320)
  quality = (low) (medium) [high] [best]
  refresh rate = 240 (30..240)
```

Write the complete header to `solution.hpp` in the current directory.
