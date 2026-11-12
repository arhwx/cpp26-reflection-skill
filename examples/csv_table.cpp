// csv_table: a block of text becomes a typed table, like an f# type provider. the header names become
// member names, the column types are inferred from the data, and the rows are parsed by the compiler
#include <algorithm>
#include <array>
#include <meta>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

namespace csv {
using std::meta::info;
using grid = std::vector<std::vector<std::string_view>>;

consteval std::string count(std::size_t n) { return (n > 9 ? count(n / 10) : "") + char('0' + n % 10); }

consteval grid cells(std::string_view text) {  // row 0 is the header
    grid out;
    for (auto line : std::views::split(text, '\n')) {
        if (std::ranges::empty(line)) continue;
        auto& row = out.emplace_back();
        for (auto cell : std::views::split(line, ',')) row.emplace_back(cell);
        if (row.size() != out[0].size())
            throw std::meta::exception("csv: row " + count(out.size() - 1) + " has " + count(row.size()) + " cells, the header has " +
                                       count(out[0].size()), ^^cells);
    }
    return out;
}

constexpr bool is_number(std::string_view s, int dots_allowed) {
    if (s.starts_with('-')) s.remove_prefix(1);
    int dots = std::ranges::count(s, '.');
    return !s.empty() && dots <= dots_allowed && std::ranges::all_of(s, [](char c) { return c == '.' || (c >= '0' && c <= '9'); });
}
constexpr double number(std::string_view s) {  // floating from_chars is not constexpr
    double v = 0, scale = 1;
    for (bool fraction = false; char c : s) {
        if (c == '-') scale = -scale;
        else if (c == '.') fraction = true;
        else v = v * 10 + (c - '0'), scale *= fraction ? 10 : 1;
    }
    return v / scale;
}
constexpr bool is_flag(std::string_view s) { return s == "yes" || s == "no"; }

consteval info column_type(const grid& g, std::size_t col) {  // the narrowest type that fits every cell
    auto all = [&](auto fits) { return std::ranges::all_of(g | std::views::drop(1), [&](const auto& row) { return fits(row[col]); }); };
    if (all(is_flag)) return ^^bool;
    if (all([](std::string_view s) { return is_number(s, 0); })) return ^^int;
    if (all([](std::string_view s) { return is_number(s, 1); })) return ^^double;
    return ^^const char*;
}

template <fixed_string Text>
struct table {
    struct row;  // one member per column, named after the header
    consteval {
        grid g = cells(Text.view());
        std::vector<info> specs;
        for (std::size_t c = 0; c < g[0].size(); ++c) specs.push_back(data_member_spec(column_type(g, c), {.name = g[0][c]}));
        define_aggregate(^^row, specs);
    }
    static constexpr auto columns = std::define_static_array(nonstatic_data_members_of(^^row, std::meta::access_context::unprivileged()));
    static constexpr std::size_t size = cells(Text.view()).size() - 1;
    static constexpr std::array<row, size> rows = [] consteval {
        std::array<row, size> out{};
        grid g = cells(Text.view());
        for (std::size_t r = 0; r < size; ++r) {
            std::size_t c = 0;
            template for (constexpr info m : columns) {
                std::string_view cell = g[r + 1][c++];
                if constexpr (type_of(m) == ^^bool) out[r].[:m:] = cell == "yes";
                else if constexpr (type_of(m) == ^^int) out[r].[:m:] = static_cast<int>(number(cell));
                else if constexpr (type_of(m) == ^^double) out[r].[:m:] = number(cell);
                else out[r].[:m:] = std::define_static_string(cell);
            }
        }
        return out;
    }();
    static consteval std::string_view schema() {
        std::string out;
        for (info m : columns) {
            info t = type_of(m);
            if (!out.empty()) out += " ";
            out += std::string(identifier_of(m)) + ": " + (t == ^^bool ? "bool" : t == ^^int ? "int" : t == ^^double ? "double" : "text");
        }
        return std::define_static_string(out);
    }
};

consteval std::string_view problem(std::string_view text) {  // what is wrong with the text, or "ok"
    try {
        cells(text);
        return "ok";
    } catch (const std::meta::exception& e) {
        return std::define_static_string(std::string_view(e.what()));
    }
}
}  // namespace csv

// ---- user code
using planets = csv::table<R"(name,moons,mass,ringed
mercury,0,0.055,no
venus,0,0.815,no
earth,1,1,no
mars,2,0.107,no
jupiter,95,317.8,yes
saturn,146,95.2,yes
)">;

static_assert(std::is_same_v<decltype(planets::row::moons), int> && std::is_same_v<decltype(planets::row::mass), double>);
static_assert(planets::rows[2].moons == 1 && planets::rows[5].ringed);  // the data is a constant

int main() {
    std::println("{}", planets::schema());
    int moons = 0;
    for (const planets::row& p : planets::rows) moons += p.moons;  // typed member access, p.mooons would not compile
    std::println("{} planets, {} moons", planets::size, moons);
    const planets::row& heavy = std::ranges::max(planets::rows, {}, &planets::row::mass);
    std::println("heaviest: {} at {} earth masses", heavy.name, heavy.mass);
    for (const planets::row& p : planets::rows)
        if (p.ringed) std::println("ringed: {}", p.name);
    std::println("{}", csv::problem("a,b,c\n1,2,3\n4,5\n"));
}

// output:
// name: text moons: int mass: double ringed: bool
// 6 planets, 244 moons
// heaviest: jupiter at 317.8 earth masses
// ringed: jupiter
// ringed: saturn
// csv: row 2 has 2 cells, the header has 3
