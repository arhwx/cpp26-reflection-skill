#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <meta>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

namespace csv {
using std::meta::info;
using grid = std::vector<std::vector<std::string_view>>;

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

consteval std::string count(std::size_t n) { return (n > 9 ? count(n / 10) : "") + char('0' + n % 10); }

// row 0 is the header, empty lines are skipped
consteval grid cells(std::string_view text) {
    grid out;
    for (auto line : std::views::split(text, '\n')) {
        if (std::ranges::empty(line)) continue;
        auto& row = out.emplace_back();
        for (auto cell : std::views::split(line, ',')) row.emplace_back(cell);
    }
    return out;
}

// "ok", or the first row whose cell count differs from the header
consteval std::string_view problem(std::string_view text) {
    grid g = cells(text);
    for (std::size_t r = 1; r < g.size(); ++r)
        if (g[r].size() != g[0].size())
            return std::define_static_string("csv: row " + count(r) + " has " + count(g[r].size()) + " cells, the header has " +
                                             count(g[0].size()));
    return "ok";
}

constexpr bool is_number(std::string_view s, int dots_allowed) {
    if (s.starts_with('-')) s.remove_prefix(1);
    auto dots = std::ranges::count(s, '.');
    return !s.empty() && s != "." && dots <= dots_allowed &&
           std::ranges::all_of(s, [](char c) { return c == '.' || (c >= '0' && c <= '9'); });
}
// floating from_chars is not constexpr. digits / 10^k is correctly rounded while both are exact
constexpr double number(std::string_view s) {
    double v = 0, scale = 1;
    bool negative = false;
    for (bool fraction = false; char c : s) {
        if (c == '-') negative = true;
        else if (c == '.') fraction = true;
        else v = v * 10 + (c - '0'), scale *= fraction ? 10 : 1;
    }
    return (negative ? -v : v) / scale;
}
constexpr bool is_flag(std::string_view s) { return s == "yes" || s == "no"; }

// the narrowest type that fits every cell of the column
consteval info column_type(const grid& g, std::size_t col) {
    auto all = [&](auto fits) { return std::ranges::all_of(g | std::views::drop(1), [&](const auto& row) { return fits(row[col]); }); };
    if (all(is_flag)) return ^^bool;
    if (all([](std::string_view s) { return is_number(s, 0); })) return ^^int;
    if (all([](std::string_view s) { return is_number(s, 1); })) return ^^double;
    return ^^const char*;
}

template <fixed_string Text>
struct table {
    static_assert(problem(Text.view()) == "ok", problem(Text.view()));
    struct row;
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
            if (!out.empty()) out += ", ";
            out += std::string(identifier_of(m)) + ": " + (t == ^^bool ? "bool" : t == ^^int ? "int" : t == ^^double ? "double" : "text");
        }
        return std::define_static_string(out);
    }
};
}  // namespace csv
