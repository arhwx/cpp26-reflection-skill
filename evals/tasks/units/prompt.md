I want physical units checked by the type system, with the units written as text: `units::unit<si, "kg*m/s^2">` should be a type, adding meters to seconds should not compile, and kilometers per hour should convert on the way in and out. The base dimensions and the named units come from one enum that I write myself, so the library knows no unit names of its own.

Everything goes in `namespace units`:
- `units::derived`: an annotation for an enumerator. `[[=units::derived("text")]]` defines a named unit with factor 1, and `[[=units::derived(f, "text")]]` (`f` is a number, converted to `double`) one with factor `f`. The unit `text` follows the syntax below and only uses enumerators declared before it.
- A *system* is an enum. Its enumerators without a `units::derived` annotation are the base dimensions, in declaration order. The enumerators with one are named units.
- `template <class Sys, int... E> struct units::quantity { double value; };`: an aggregate with exactly this one public member. There is one exponent per base dimension of `Sys`, in their order.
- `units::unit<Sys, "text">`: an alias for the `quantity` type of that unit text. Scale factors do not change the type, so `unit<si, "km/h">` is `unit<si, "m/s">`.
- `units::make<Sys, "text">(double v)`, usable in constant expressions: the quantity `unit<Sys, "text">` holding `v * f`, where `f` is the factor of the text.
- `units::in<"text">(q)` returns `q.value / f` as a `double`, where `f` is the factor of the text in the system of `q`. It takes part in overload resolution only when `q`'s type is `unit<Sys, "text">`, so asking for the wrong dimension is not a valid expression.
- `std::string units::to_string(q)`: the value formatted with `std::format("{}", value)`, then a space and the unit text of `q`'s type, or the value alone when every exponent is 0. The unit text lists the base dimensions with a positive exponent in declaration order, joined by `*`, each written as `name` or `name^k`. Then for each base dimension with a negative exponent, in declaration order, it appends `/name` or `/name^k` with `k` the absolute exponent. When there are negative exponents but no positive ones, the text starts with `1`, as in `1/s`.
- Operators: `+` and `-` take two quantities of the same type and are not valid for different types. `*` and `/` take two quantities of the same system and add or subtract the exponents. A quantity can also be multiplied by a `double` on either side or divided by a `double`, and `double / q` gives the inverse exponents. All of them compute the plain `double` operation on the values.
- `template <class Sys> consteval std::string_view units::check(std::string_view text)`: `"ok"` when the text is valid in `Sys`, otherwise the first error, as below. `unit` and `make` with an invalid text must not compile.

Unit text:
- A text is one or more terms separated by `*` or `/`, read left to right, with no spaces. A term is the character `1` or a name, optionally followed by `^` and an exponent: an optional `-` and one or more decimal digits. A name is the longest run of letters and `_`. Names are case sensitive.
- A name is an enumerator of the system. A base dimension has exponent 1 for itself and factor 1. A named unit has the exponents of its annotation text, and its factor is the annotation's factor times the factor of its text. The term `1` has all exponents 0 and factor 1.
- A term with exponent `k` (1 when no `^` is written) counts as `-k` when it follows `/`. Its exponents times that count are added to the result.
- The factor starts at `1.0` and is updated for every term in order: with count `c > 0` it is multiplied by the term's factor `c` times, with `c < 0` it is divided by it `-c` times.
- Errors: a name that is not an enumerator of the system gives `unknown unit 'name'`. When a term or an exponent's digits are missing, or a term is followed by anything other than `*`, `/` or the end, the error is `syntax error at N`, with `N` the 0-based position of the offending character (the length of the text when the text ends too early). The first error found reading left to right is reported.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string_view>
#include <type_traits>

enum class si {
    m,
    kg,
    s,
    A,
    N [[=units::derived("kg*m/s^2")]],
    J [[=units::derived("N*m")]],
    W [[=units::derived("J/s")]],
    Hz [[=units::derived("1/s")]],
    km [[=units::derived(1000, "m")]],
    g [[=units::derived(0.001, "kg")]],
    min [[=units::derived(60, "s")]],
    h [[=units::derived(60, "min")]],
    kWh [[=units::derived(1000, "W*h")]],
    C [[=units::derived("A*s")]],
    V [[=units::derived("W/A")]],
};

using meters = units::unit<si, "m">;
using seconds = units::unit<si, "s">;
using newtons = units::unit<si, "N">;

static_assert(std::is_same_v<newtons, units::quantity<si, 1, 1, -2, 0>>);
static_assert(std::is_same_v<units::unit<si, "kg*m/s^2">, units::unit<si, "m*kg*s^-2">>);
static_assert(std::is_same_v<units::unit<si, "J">, units::unit<si, "N*m">>);
static_assert(std::is_same_v<units::unit<si, "kWh">, units::unit<si, "J">>);
static_assert(std::is_same_v<units::unit<si, "km/h">, units::unit<si, "m/s">>);
static_assert(std::is_same_v<units::unit<si, "m/m">, units::quantity<si, 0, 0, 0, 0>>);
static_assert(std::is_same_v<units::unit<si, "Hz*s">, units::unit<si, "1">>);
static_assert(std::is_same_v<units::unit<si, "V">, units::quantity<si, 2, 1, -3, -1>>);
static_assert(std::is_same_v<units::unit<si, "1/s/s">, units::unit<si, "s^-2">>);
static_assert(std::is_same_v<units::unit<si, "m/s*s">, meters>);
static_assert(units::make<si, "km">(1.5).value == 1500);

template <class A, class B>
concept addable = requires(A a, B b) { a + b; };
template <class Q>
concept in_kmh = requires(Q q) { units::in<"km/h">(q); };
template <class Q>
concept in_kwh = requires(Q q) { units::in<"kWh">(q); };

int main() {
    auto mass = units::make<si, "kg">(2.0);
    auto accel = units::make<si, "m/s^2">(9.5);
    newtons force = mass * accel;
    std::println("force: {}", units::to_string(force));
    std::println("force + 1 N: {}", units::to_string(force + units::make<si, "N">(1)));

    auto dist = units::make<si, "km">(3.0);
    auto time = units::make<si, "min">(2.5);
    auto speed = dist / time;
    std::println("dist: {}, time: {}", units::to_string(dist), units::to_string(time));
    std::println("speed: {} = {} km/h", units::to_string(speed), units::in<"km/h">(speed));
    std::println("90 km/h: {}", units::to_string(units::make<si, "km/h">(90)));
    std::println("54 km/h in m/s: {}", units::in<"m/s">(units::make<si, "km/h">(54)));

    auto energy = force * dist;
    auto power = energy / time;
    std::println("energy: {} = {} kWh", units::to_string(energy), units::in<"kWh">(energy));
    std::println("power: {}", units::to_string(power));
    std::println("1 kWh: {}", units::to_string(units::make<si, "kWh">(1)));
    std::println("voltage: {}", units::to_string(power / units::make<si, "A">(4)));
    std::println("charge: {}", units::to_string(units::make<si, "A">(2) * units::make<si, "s">(3)));
    std::println("frequency: {}", units::to_string(1.0 / units::make<si, "min">(0.5)));
    std::println("50 Hz: {}", units::to_string(units::make<si, "Hz">(50)));
    std::println("ratio: {}", units::to_string(dist / units::make<si, "m">(500)));
    std::println("mass in g: {}", units::in<"g">(mass));
    std::println("90 min in h: {}", units::in<"h">(units::make<si, "min">(90)));
    std::println("half force: {}", units::to_string(force / 2.0));
    std::println("braking: {}", units::to_string(-2.0 * accel));
    std::println("area: {}", units::to_string(units::make<si, "m^2*kg/kg">(4) * 0.5));

    std::println("addable m + m: {}", addable<meters, meters>);
    std::println("addable m + s: {}", addable<meters, seconds>);
    std::println("addable J + N*m: {}", addable<units::unit<si, "J">, units::unit<si, "N*m">>);
    std::println("speed in km/h: {}", in_kmh<decltype(speed)>);
    std::println("distance in km/h: {}", in_kmh<decltype(dist)>);
    std::println("energy in kWh: {}", in_kwh<decltype(energy)>);
    std::println("power in kWh: {}", in_kwh<decltype(power)>);

    constexpr std::string_view checks[] = {
        units::check<si>("kg*m/s^2"), units::check<si>("N^-2/A*Hz^0"), units::check<si>("kg*lb"),
        units::check<si>("kg**m"),    units::check<si>("m/s^"),        units::check<si>(""),
        units::check<si>("m s"),      units::check<si>("s^x"),         units::check<si>("10*m"),
        units::check<si>("Km"),
    };
    for (std::string_view c : checks) std::println("check: {}", c);
}
```

It must print exactly:

```
force: 19 m*kg/s^2
force + 1 N: 20 m*kg/s^2
dist: 3000 m, time: 150 s
speed: 20 m/s = 72 km/h
90 km/h: 25 m/s
54 km/h in m/s: 15
energy: 57000 m^2*kg/s^2 = 0.015833333333333335 kWh
power: 380 m^2*kg/s^3
1 kWh: 3600000 m^2*kg/s^2
voltage: 95 m^2*kg/s^3/A
charge: 6 s*A
frequency: 0.03333333333333333 1/s
50 Hz: 50 1/s
ratio: 6
mass in g: 2000
90 min in h: 1.5
half force: 9.5 m*kg/s^2
braking: -19 m/s^2
area: 2 m^2
addable m + m: true
addable m + s: false
addable J + N*m: true
speed in km/h: true
distance in km/h: false
energy in kWh: true
power in kWh: false
check: ok
check: ok
check: unknown unit 'lb'
check: syntax error at 3
check: syntax error at 4
check: syntax error at 0
check: syntax error at 1
check: syntax error at 2
check: syntax error at 1
check: unknown unit 'Km'
```

Write the complete header to `solution.hpp` in the current directory.
