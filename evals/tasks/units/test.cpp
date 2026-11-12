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
