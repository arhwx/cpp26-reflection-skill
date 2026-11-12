I want Julia-style multiple dispatch over ordinary C++ overloads: a call picks the overload that best fits the dynamic types of all its arguments, not their static types. The overloads are plain functions in a namespace and nothing is registered by hand. A compile-time check should also tell me when two overloads can be ambiguous, and which overload to add.

Everything goes in `namespace multi`:
- `multi::call<NS, "name">(args...)`: `NS` is a reflection of a namespace and the second template argument is a string literal. You define the class type that the literal converts to. The function parameters are `A&... args`, deduced from lvalue arguments. It calls one overload as described below and returns its result.
- `multi::scans`: an `int` variable that starts at 0 and counts how many times `call` had to choose an overload, as described below.
- `consteval std::string_view multi::check(std::meta::info ns, std::string_view name)` (`constexpr` is fine too): an empty string when the overloads of `name` in `ns` cannot be ambiguous, otherwise a message, as described below.

Overloads:
- The overloads of a name are all functions with that name declared directly in the namespace.
- Every parameter of an overload has type `C&` or `const C&`, where `C` is a polymorphic class. The classes of an overload are its parameter types without the reference and the `const`.
- All overloads of a name have the same return type and the same number of parameters, and `call` gets that many arguments.
- An overload `f` covers an overload `g` when each class of `f` is the same as, or a base of, the class of `g` at the same position.

What `call` does:
- An overload is applicable when each of its classes is the same as, or a public base of, the dynamic type of the argument at the same position.
- `call` calls the applicable overload that every other applicable overload covers: the most specific one. When `check` returns an empty string, there is at most one such overload. Each argument is passed as the overload's parameter type, so the overload sees the object it was given.
- When no overload is applicable, `call` throws `std::logic_error` with the message `no applicable method`.
- `call` remembers its choice for each combination of dynamic argument types, separately for each namespace, name and list of static argument types. When the combination was seen before, it calls the remembered overload. Otherwise it searches the overloads, remembers the choice and adds 1 to `multi::scans`. A call that throws does not change `multi::scans`.
- `call` for a name whose `check` is not empty must not compile.

What `check` reports:
- Two different overloads `a` and `b` can match the same call when, at each position, one of their two classes is the same as or a base of the other. Their meet is the list of the more derived class at each position.
- The overloads are a problem when they can match the same call, neither covers the other, and no overload has exactly their meet as its classes.
- `check` looks at the overloads `a` in declaration order and, for each one, at the other overloads `b` in declaration order. For the first problem pair it returns `ambiguous: <a> and <b>; define <meet>`. Each of the three is spelled as the function name followed by its class names in parentheses, separated by `, `, like `meet(Left, Base)`. Class names are unqualified.
- With no problem pair it returns an empty string.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
asteroid x asteroid -> they bounce
asteroid x ship     -> the ship is damaged
asteroid x fighter  -> the ship is damaged
ship     x asteroid -> the ship is damaged
ship     x ship     -> the ships dock
ship     x fighter  -> the ships dock
fighter  x asteroid -> the fighter shoots the asteroid
fighter  x ship     -> the ships dock
fighter  x fighter  -> the ships dock
18 calls, 9 overload scans
crew of the asteroid: 0
crew of the ship: 3
crew of the fighter: 1
12 overload scans
crew of a fighter seen as a ship: 1
refuel: refueled
refuel failed: no applicable method
check collide: ""
check crew: ""
ambiguous: meet(Left, Base) and meet(Base, Left); define meet(Left, Left)
ambiguous: touch(Left, Base) and touch(Base, Right); define touch(Left, Right)
```

Write the complete header to `solution.hpp` in the current directory.
