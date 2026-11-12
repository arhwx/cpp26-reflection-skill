// units: f#-style units of measure. unit text such as "kg*m/s^2" becomes a type, named and scaled units are
// annotations on an enum, and adding meters to seconds does not compile
#include <algorithm>
#include <charconv>
#include <format>
#include <meta>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace units {
using std::meta::info;

struct derived {  // the annotation for a named unit: a factor times a unit text
    double factor;
    const char* text;
    consteval derived(std::string_view t) : derived(1, t) {}
    consteval derived(double f, std::string_view t) : factor(f), text(std::define_static_string(t)) {}
};

template <std::size_t N>
struct text {  // a string literal as a template argument
    char data[N]{};
    consteval text(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr operator std::string_view() const { return {data, N - 1}; }
};

template <class Sys, int... E>  // a value with one exponent per base dimension of Sys
struct quantity {
    double value;
    friend constexpr quantity operator+(quantity a, quantity b) { return {a.value + b.value}; }
    template <int... F>
    constexpr quantity<Sys, (E + F)...> operator*(quantity<Sys, F...> q) const { return {value * q.value}; }
    template <int... F>
    constexpr quantity<Sys, (E - F)...> operator/(quantity<Sys, F...> q) const { return {value / q.value}; }
};

struct dim {  // what a unit text means: one exponent per base dimension and a scale factor
    std::vector<int> exps;
    double factor = 1;
};
[[noreturn]] consteval void fail(std::string why) { throw std::meta::exception(why, ^^dim); }
consteval std::string digits(std::size_t n) { return (n > 9 ? digits(n / 10) : "") + char('0' + n % 10); }

consteval std::vector<info> bases(info sys) {  // the enumerators without an annotation
    std::vector<info> out;
    for (info e : enumerators_of(sys))
        if (annotations_of_with_type(e, ^^derived).empty()) out.push_back(e);
    return out;
}

consteval dim parse(info sys, std::string_view s);
consteval dim named(info sys, std::string_view name) {  // a base dimension, a named unit or 1
    std::vector<info> base = bases(sys);
    dim d{std::vector<int>(base.size())};
    if (name == "1") return d;
    for (info e : enumerators_of(sys)) {
        if (identifier_of(e) != name) continue;
        std::vector<info> defs = annotations_of_with_type(e, ^^derived);
        if (defs.empty()) return d.exps[std::ranges::find(base, e) - base.begin()] = 1, d;
        derived def = extract<derived>(defs[0]);
        d = parse(sys, def.text);
        return d.factor *= def.factor, d;
    }
    fail("unknown unit '" + std::string(name) + "'");
}

// terms joined by * and /, read left to right. a term is a name with an optional ^power
consteval dim parse(info sys, std::string_view s) {
    dim out{std::vector<int>(bases(sys).size())};
    std::size_t at = 0;
    auto syntax = [&] { fail("syntax error at " + digits(at)); };
    auto letter = [](char c) { return c == '_' || c == '1' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
    for (int sign = 1;;) {
        std::size_t from = at;
        while (at < s.size() && letter(s[at])) ++at;
        if (at == from) syntax();
        dim term = named(sys, s.substr(from, at - from));
        int k = 1;
        if (at < s.size() && s[at] == '^') {
            auto [end, error] = std::from_chars(s.data() + at + 1, s.data() + s.size(), k);
            at = end - s.data();
            if (error != std::errc{}) syntax();
        }
        k *= sign;
        for (std::size_t i = 0; i < out.exps.size(); ++i) out.exps[i] += k * term.exps[i];
        for (int i = 0; i < k; ++i) out.factor *= term.factor;
        for (int i = 0; i > k; --i) out.factor /= term.factor;
        if (at == s.size()) return out;
        if (s[at] != '*' && s[at] != '/') syntax();
        sign = s[at++] == '*' ? 1 : -1;
    }
}

consteval info quantity_of(info sys, std::string_view s) {  // the type quantity<Sys, exponents...>
    std::vector<info> args{sys};
    for (int e : parse(sys, s).exps) args.push_back(std::meta::reflect_constant(e));
    return substitute(^^quantity, args);
}

template <class Sys, text U>
using unit = [:quantity_of(^^Sys, U):];
template <class Sys, text U>
constexpr double factor = parse(^^Sys, U).factor;
template <class Sys, text U>
constexpr unit<Sys, U> make(double v) { return {v * factor<Sys, U>}; }

template <text U, class Sys, int... E>  // the value of q in unit U, only when U has the dimension of q
    requires std::is_same_v<quantity<Sys, E...>, unit<Sys, U>>
constexpr double in(quantity<Sys, E...> q) { return q.value / factor<Sys, U>; }

consteval std::string unit_text(info sys, std::vector<int> exps) {  // like "m^2*kg/s^3" or "1/s"
    std::vector<info> base = bases(sys);
    std::string up, down;
    for (std::size_t i = 0; i < exps.size(); ++i) {
        std::string term(identifier_of(base[i]));
        int k = exps[i] < 0 ? -exps[i] : exps[i];
        if (k > 1) term += "^" + digits(k);
        if (exps[i] > 0) up += (up.empty() ? "" : "*") + term;
        if (exps[i] < 0) down += "/" + term;
    }
    return up.empty() && !down.empty() ? "1" + down : up + down;
}

template <class Sys>
consteval std::string_view check(std::string_view s) {  // "ok", or why s is not a unit of Sys
    try {
        parse(^^Sys, s);
        return "ok";
    } catch (const std::meta::exception& e) {
        return std::define_static_string(std::string_view(e.what()));
    }
}
}  // namespace units

template <class Sys, int... E>
struct std::formatter<units::quantity<Sys, E...>> : std::formatter<double> {
    auto format(units::quantity<Sys, E...> q, auto& ctx) const {
        static constexpr std::string_view suffix = std::define_static_string(units::unit_text(^^Sys, {E...}));
        auto out = std::formatter<double>::format(q.value, ctx);
        return suffix.empty() ? out : std::format_to(out, " {}", suffix);
    }
};

// ---- user code. f#: [<Measure>] type N = kg m / s^2 and let force = 1200.0<kg> * 3.0<m/s^2>
enum class si {
    m, kg, s, A,  // the base dimensions
    N [[=units::derived("kg*m/s^2")]],
    J [[=units::derived("N*m")]],
    W [[=units::derived("J/s")]],
    V [[=units::derived("W/A")]],
    km [[=units::derived(1000, "m")]],
    min [[=units::derived(60, "s")]],
    h [[=units::derived(60, "min")]],
    kWh [[=units::derived(1000, "W*h")]],
};
using units::unit;

static_assert(std::is_same_v<unit<si, "N">, unit<si, "kg*m/s^2">>);
static_assert(std::is_same_v<unit<si, "kWh">, unit<si, "N*m">>);
static_assert(std::is_same_v<unit<si, "km/h">, unit<si, "m/s">>);  // a scale factor does not change the type
static_assert(std::is_same_v<unit<si, "V">, units::quantity<si, 2, 1, -3, -1>>);  // exponents of m, kg, s, A
static_assert(units::in<"min">(units::make<si, "h">(2)) == 120);  // conversions also run at compile time

template <class A, class B>
concept addable = requires(A a, B b) { a + b; };
static_assert(addable<unit<si, "m">, unit<si, "km">>);
static_assert(!addable<unit<si, "m">, unit<si, "s">>);  // meters plus seconds does not compile

int main() {
    auto mass = units::make<si, "kg">(1200);
    auto accel = units::make<si, "m/s^2">(3);
    unit<si, "N"> force = mass * accel;  // kg times m/s^2 is a newton
    auto dist = units::make<si, "km">(1);  // stored as 1000 m
    auto time = units::make<si, "s">(50);
    auto speed = dist / time;
    auto energy = force * dist;
    auto power = energy / time;
    std::println("force: {}", force);
    std::println("speed: {} = {} km/h", speed, units::in<"km/h">(speed));
    std::println("energy: {} = {} kWh", energy, units::in<"kWh">(energy));
    std::println("power: {}", power);
    std::println("current at 400 V: {}", power / units::make<si, "V">(400));
    // units::in<"kWh">(power) does not compile: power is not an energy
    std::println("kg*lb: {}", units::check<si>("kg*lb"));
    std::println("kg**m: {}", units::check<si>("kg**m"));
}

// output:
// force: 3600 m*kg/s^2
// speed: 20 m/s = 72 km/h
// energy: 3600000 m^2*kg/s^2 = 1 kWh
// power: 72000 m^2*kg/s^3
// current at 400 V: 180 A
// kg*lb: unknown unit 'lb'
// kg**m: syntax error at 3
