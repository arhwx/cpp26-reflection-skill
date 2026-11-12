#include "solution.hpp"

#include <memory>
#include <print>
#include <stdexcept>
#include <string>

// plain classes and plain overloads, nothing is registered
namespace space {
struct Thing {
    virtual ~Thing() = default;
};
struct Asteroid : Thing {};
struct Ship : Thing {};
struct Fighter : Ship {};

std::string collide(const Thing&, const Thing&) { return "they bounce"; }
std::string collide(const Asteroid&, const Ship&) { return "the ship is damaged"; }
std::string collide(const Ship&, const Asteroid&) { return "the ship is damaged"; }
std::string collide(const Ship&, const Ship&) { return "the ships dock"; }
std::string collide(const Fighter&, const Asteroid&) { return "the fighter shoots the asteroid"; }

int crew(const Thing&) { return 0; }
int crew(Ship&) { return 3; }
int crew(const Fighter&) { return 1; }

std::string refuel(const Ship&, const Ship&) { return "refueled"; }
}  // namespace space

namespace broken {
struct Base {
    virtual ~Base() = default;
};
struct Left : Base {};
struct Right : Base {};
int meet(const Left&, const Base&);
int meet(const Base&, const Left&);
int touch(const Left&, const Base&);
int touch(const Base&, const Right&);
int touch(const Left&, const Left&);
}  // namespace broken

int main() {
    using namespace space;
    std::unique_ptr<Thing> things[] = {std::make_unique<Asteroid>(), std::make_unique<Ship>(), std::make_unique<Fighter>()};
    const char* names[] = {"asteroid", "ship", "fighter"};
    for (int round = 0; round < 2; ++round)
        for (int i = 0; i < 3; ++i)
            for (int j = 0; j < 3; ++j) {
                std::string what = multi::call<^^space, "collide">(*things[i], *things[j]);
                if (round == 0) std::println("{:8} x {:8} -> {}", names[i], names[j], what);
            }
    std::println("18 calls, {} overload scans", multi::scans);

    for (int i = 0; i < 3; ++i) std::println("crew of the {}: {}", names[i], multi::call<^^space, "crew">(*things[i]));
    std::println("{} overload scans", multi::scans);
    Fighter fighter;
    Ship& as_ship = fighter;
    std::println("crew of a fighter seen as a ship: {}", multi::call<^^space, "crew">(as_ship));

    std::println("refuel: {}", multi::call<^^space, "refuel">(*things[2], *things[1]));
    try {
        multi::call<^^space, "refuel">(*things[0], *things[1]);
    } catch (const std::logic_error& e) {
        std::println("refuel failed: {}", e.what());
    }

    std::println("check collide: \"{}\"", multi::check(^^space, "collide"));
    std::println("check crew: \"{}\"", multi::check(^^space, "crew"));
    std::println("{}", multi::check(^^broken, "meet"));
    std::println("{}", multi::check(^^broken, "touch"));
}
