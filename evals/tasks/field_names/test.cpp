#include "solution.hpp"

#include <array>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>

struct Config {
    int port;
    std::string host;
    static inline int instances = 0;
    bool verbose;
    void reset() {}
    using size_type = unsigned;
    double timeout;
};

struct Empty {};

struct Flags {
    unsigned read : 1;
    unsigned write : 1;
    unsigned char mode;
};

constexpr auto names = field_names<Config>();
static_assert(std::is_same_v<decltype(names), const std::array<std::string_view, 4>>);
static_assert(names[0] == "port" && names[1] == "host" && names[2] == "verbose" && names[3] == "timeout");
static_assert(field_names<Empty>().empty());
static_assert(field_names<Flags>().size() == 3 && field_names<Flags>()[2] == "mode");

int main() {
    for (std::string_view n : names)
        std::println("{}", n);
    for (std::string_view n : field_names<Flags>())
        std::println("{}", n);
    std::println("{}", field_names<Empty>().size());
}
