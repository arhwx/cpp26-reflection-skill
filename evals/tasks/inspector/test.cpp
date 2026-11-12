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
