#include "solution.hpp"

#include <optional>
#include <print>
#include <string_view>

enum class Color { red, green, blue };
enum class Status : short { ok = 0, warn = 2, error = 5, fatal = -1 };
enum Legacy { first = 1, second = 4 };

namespace net {
enum class Proto : unsigned char { tcp = 6, udp = 17 };
}

static_assert(enum_name(Color::blue) == "blue");
static_assert(enum_from_name<Status>("fatal") == Status::fatal);

int main() {
    Status values[] = {Status::ok, Status::warn, static_cast<Status>(3), Status::error, Status::fatal};
    for (Status s : values)
        std::println("{} -> [{}]", static_cast<int>(s), enum_name(s));

    std::println("{} {} {}", enum_name(Color::green), enum_name(second), enum_name(net::Proto::udp));
    std::println("[{}]", enum_name(static_cast<Legacy>(2)));

    std::string_view names[] = {"warn", "fatal", "ok", "Warn", "unknown", ""};
    for (std::string_view n : names) {
        std::optional<Status> s = enum_from_name<Status>(n);
        if (s)
            std::println("[{}] -> {}", n, static_cast<int>(*s));
        else
            std::println("[{}] -> nullopt", n);
    }

    std::optional<net::Proto> p = enum_from_name<net::Proto>("tcp");
    std::println("tcp -> {}", p ? static_cast<int>(*p) : -1);
}
