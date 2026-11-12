#include "solution.hpp"

#include <cstdint>
#include <print>

enum class Perm : unsigned {
    none = 0,
    read = 1u << 0,
    write = 1u << 1,
    exec = 1u << 2,
    admin = 1u << 3,
    all = 0xFu,
};

enum class Opt : std::uint8_t { fast = 1, small = 2, debug = 16 };

int main() {
    for (unsigned v : {0u, 1u, 3u, 5u, 15u, 8u, 0x21u})
        std::println("{} -> {}", v, flags_to_string(static_cast<Perm>(v)));
    std::println("{}", flags_to_string(Perm::all));
    std::println("{}", flags_to_string(static_cast<Opt>(17)));
    std::println("{}", flags_to_string(Opt{}));
}
