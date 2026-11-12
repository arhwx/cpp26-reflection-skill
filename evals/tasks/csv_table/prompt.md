We keep small reference tables as CSV text in our source. I want the compiler to read that text: the header names become member names, each column gets a type inferred from its cells, and the rows become a constant array, with no code generator in the build.

Everything goes in `namespace csv`:
- `csv::table<"...">`: a class template whose only template argument is the CSV text, given as a string literal (raw string literals included, as in the test).
- `table<Text>::row`: an aggregate struct with one public non-static data member per column, named after the header cell, in column order, and no other non-static data members.
- `table<Text>::size`: a `static constexpr std::size_t`, the number of data rows.
- `table<Text>::rows`: a `static constexpr std::array<row, size>` holding the data rows in order. It must be usable in constant expressions, as the `static_assert`s in the test do.
- `table<Text>::schema()`: a static function returning `std::string_view` (it may be `consteval`) that describes the columns.
- `csv::problem(std::string_view text)`: a function returning `std::string_view` (it may be `consteval`) that says whether `text` is a well-formed table.

Reading the text:
- Lines are separated by `'\n'`. Empty lines are ignored anywhere, including a leading or trailing one. The first non-empty line is the header, every later non-empty line is a data row.
- Cells are separated by `','`. There is no quoting, escaping or trimming of spaces. The test's header names are valid identifiers.

Column types: a column's type depends on all of its data cells.
- `bool` if every cell is `yes` or `no`. `yes` is `true`.
- otherwise `int` if every cell is a whole number: an optional `-`, then one or more digits.
- otherwise `double` if every cell is a decimal number: an optional `-`, then digits with at most one `.` among them. The value is the `double` nearest to the decimal value, as `std::from_chars` would give. So the `1` in the `mass` column does not make that column `int`, it is just the double `1.0`.
- otherwise text, stored as `const char*` pointing to a null-terminated copy of the cell with static storage duration.

`schema()` returns `name: type` for each column, in column order, joined with `, `. The type is written `bool`, `int`, `double` or `text`.

`problem(text)` returns `ok` if every data row has as many cells as the header. Otherwise it describes the first data row that does not, as `csv: row <r> has <n> cells, the header has <h>`, where data rows are numbered from 1 in order and empty lines are not counted. Numbers are written in decimal without padding.

Using `table` with text that `problem` rejects must not compile, but the test does not do that.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
name: text, moons: int, mass: double, ringed: bool
6 planets, 244 moons
heaviest: jupiter at 317.8 earth masses
ringed: jupiter
ringed: saturn
station: text, temp: double, count: int, ok: bool, note: text
north -3.5 -2 true cold
south 12 7 false 12
east 0.25 0 true yes
3 readings, total temp 8.75
csv: row 2 has 2 cells, the header has 3
csv: row 2 has 3 cells, the header has 2
ok
```

Write the complete header to `solution.hpp` in the current directory.
