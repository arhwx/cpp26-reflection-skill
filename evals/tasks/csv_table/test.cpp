#include "solution.hpp"

#include <algorithm>
#include <print>
#include <type_traits>

using planets = csv::table<R"(name,moons,mass,ringed
mercury,0,0.055,no
venus,0,0.815,no
earth,1,1,no
mars,2,0.107,no
jupiter,95,317.8,yes
saturn,146,95.2,yes
)">;

static_assert(std::is_same_v<decltype(planets::row::name), const char*>);
static_assert(std::is_same_v<decltype(planets::row::moons), int>);
static_assert(std::is_same_v<decltype(planets::row::mass), double>);
static_assert(std::is_same_v<decltype(planets::row::ringed), bool>);
static_assert(std::is_aggregate_v<planets::row>);
static_assert(planets::rows[2].moons == 1 && planets::rows[5].ringed && !planets::rows[0].ringed);
static_assert(planets::rows[4].mass == 317.8 && planets::rows[1].mass == 0.815);

using readings = csv::table<R"(
station,temp,count,ok,note
north,-3.5,-2,yes,cold

south,12,7,no,12
east,0.25,0,yes,yes
)">;

static_assert(std::is_same_v<decltype(readings::row::temp), double>);
static_assert(std::is_same_v<decltype(readings::row::count), int>);
static_assert(std::is_same_v<decltype(readings::row::note), const char*>);
static_assert(readings::rows[0].count == -2 && readings::rows[0].temp == -3.5);

int main() {
    std::println("{}", planets::schema());
    int moons = 0;
    for (const planets::row& p : planets::rows) moons += p.moons;
    std::println("{} planets, {} moons", planets::size, moons);
    const planets::row& heavy = std::ranges::max(planets::rows, {}, &planets::row::mass);
    std::println("heaviest: {} at {} earth masses", heavy.name, heavy.mass);
    for (const planets::row& p : planets::rows)
        if (p.ringed) std::println("ringed: {}", p.name);

    std::println("{}", readings::schema());
    double total = 0;
    for (const readings::row& r : readings::rows) {
        std::println("{} {} {} {} {}", r.station, r.temp, r.count, r.ok, r.note);
        total += r.temp;
    }
    std::println("{} readings, total temp {}", readings::size, total);

    std::println("{}", csv::problem("a,b,c\n1,2,3\n4,5\n"));
    std::println("{}", csv::problem("a,b\n1,2\n\n3,4,5\n6\n"));
    std::println("{}", csv::problem("a,b\n1,2\n"));
}
