#pragma once

#include <algorithm>
#include <cstddef>
#include <format>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace units {

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

// annotation on an enumerator: a named unit defined by a factor and a unit text
struct derived {
    double factor;
    const char* text;
    consteval derived(std::string_view t) : factor(1), text(std::define_static_string(t)) {}
    consteval derived(double f, std::string_view t) : factor(f), text(std::define_static_string(t)) {}
};

// one exponent per base dimension of Sys, in declaration order
template <class Sys, int... E>
struct quantity {
    double value;
};

namespace detail {
using std::meta::info;

consteval std::optional<derived> definition(info e) {
    for (info a : std::meta::annotations_of(e))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^derived) return std::meta::extract<derived>(a);
    return std::nullopt;
}

consteval std::vector<info> base_dims(info sys) {
    std::vector<info> out;
    for (info e : std::meta::enumerators_of(sys))
        if (!definition(e)) out.push_back(e);
    return out;
}

consteval std::string number(int n) {
    if (n < 0) return "-" + number(-n);
    return (n > 9 ? number(n / 10) : "") + char('0' + n % 10);
}

struct parsed {
    std::vector<int> exps;
    double factor = 1;
    std::string error;
};

consteval bool is_name_char(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

consteval parsed parse(info sys, std::string_view text);

consteval parsed named_unit(info sys, info e) {
    std::vector<info> bases = base_dims(sys);
    if (std::optional<derived> def = definition(e)) {
        parsed p = parse(sys, def->text);
        p.factor = def->factor * p.factor;
        return p;
    }
    parsed p{std::vector<int>(bases.size()), 1.0, ""};
    for (std::size_t i = 0; i < bases.size(); ++i)
        if (bases[i] == e) p.exps[i] = 1;
    return p;
}

// terms joined by * and /, left to right; a term is 1 or a name, with an optional ^exponent
consteval parsed parse(info sys, std::string_view text) {
    std::vector<info> all = std::meta::enumerators_of(sys);
    parsed out{std::vector<int>(base_dims(sys).size()), 1.0, ""};
    auto syntax = [&](std::size_t pos) {
        out.error = "syntax error at " + number(int(pos));
        return out;
    };
    std::size_t pos = 0;
    char op = '*';
    while (true) {
        parsed term{std::vector<int>(out.exps.size()), 1.0, ""};
        if (pos < text.size() && text[pos] == '1') {
            ++pos;
        } else {
            std::size_t start = pos;
            while (pos < text.size() && is_name_char(text[pos])) ++pos;
            if (pos == start) return syntax(pos);
            std::string_view name = text.substr(start, pos - start);
            auto it = std::ranges::find_if(all, [&](info e) { return std::meta::identifier_of(e) == name; });
            if (it == all.end()) {
                out.error = "unknown unit '" + std::string(name) + "'";
                return out;
            }
            term = named_unit(sys, *it);
        }
        int k = 1;
        if (pos < text.size() && text[pos] == '^') {
            ++pos;
            bool negative = pos < text.size() && text[pos] == '-';
            if (negative) ++pos;
            std::size_t start = pos;
            k = 0;
            while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') k = k * 10 + (text[pos++] - '0');
            if (pos == start) return syntax(pos);
            if (negative) k = -k;
        }
        if (op == '/') k = -k;
        for (std::size_t i = 0; i < out.exps.size(); ++i) out.exps[i] += k * term.exps[i];
        for (int j = 0; j < (k < 0 ? -k : k); ++j) {
            if (k > 0) out.factor *= term.factor;
            else out.factor /= term.factor;
        }
        if (pos == text.size()) return out;
        if (text[pos] != '*' && text[pos] != '/') return syntax(pos);
        op = text[pos++];
    }
}

template <class Sys>
consteval info unit_type(std::string_view text) {
    parsed p = parse(^^Sys, text);
    if (!p.error.empty()) throw p.error;
    std::vector<info> args{^^Sys};
    for (int e : p.exps) args.push_back(std::meta::reflect_constant(e));
    return std::meta::substitute(^^quantity, args);
}

template <class Sys, fixed_string U>
inline constexpr double factor = parse(^^Sys, U.view()).factor;

// positive exponents joined by *, then /name for each negative one
template <class Sys>
consteval std::string_view unit_text(std::vector<int> exps) {
    std::vector<info> bases = base_dims(^^Sys);
    auto term = [&](std::size_t i, int k) {
        std::string s(std::meta::identifier_of(bases[i]));
        return k == 1 ? s : s + "^" + number(k);
    };
    std::string pos, neg;
    for (std::size_t i = 0; i < exps.size(); ++i) {
        if (exps[i] > 0) pos += (pos.empty() ? "" : "*") + term(i, exps[i]);
        if (exps[i] < 0) neg += "/" + term(i, -exps[i]);
    }
    if (pos.empty() && !neg.empty()) pos = "1";
    return std::define_static_string(pos + neg);
}

}  // namespace detail

template <class Sys, fixed_string U>
using unit = [:detail::unit_type<Sys>(U.view()):];

template <class Sys>
consteval std::string_view check(std::string_view text) {
    std::string error = detail::parse(^^Sys, text).error;
    return error.empty() ? "ok" : std::define_static_string(error);
}

template <class Sys, fixed_string U>
constexpr unit<Sys, U> make(double v) {
    return {v * detail::factor<Sys, U>};
}

template <fixed_string U, class Sys, int... E>
    requires std::is_same_v<quantity<Sys, E...>, unit<Sys, U>>
constexpr double in(quantity<Sys, E...> q) {
    return q.value / detail::factor<Sys, U>;
}

template <class Sys, int... E>
std::string to_string(quantity<Sys, E...> q) {
    static constexpr std::string_view text = detail::unit_text<Sys>({E...});
    std::string out = std::format("{}", q.value);
    if (!text.empty()) out += " " + std::string(text);
    return out;
}

template <class S, int... E>
constexpr quantity<S, E...> operator+(quantity<S, E...> a, quantity<S, E...> b) {
    return {a.value + b.value};
}
template <class S, int... E>
constexpr quantity<S, E...> operator-(quantity<S, E...> a, quantity<S, E...> b) {
    return {a.value - b.value};
}
template <class S, int... A, int... B>
constexpr quantity<S, (A + B)...> operator*(quantity<S, A...> a, quantity<S, B...> b) {
    return {a.value * b.value};
}
template <class S, int... A, int... B>
constexpr quantity<S, (A - B)...> operator/(quantity<S, A...> a, quantity<S, B...> b) {
    return {a.value / b.value};
}
template <class S, int... E>
constexpr quantity<S, E...> operator*(double a, quantity<S, E...> b) {
    return {a * b.value};
}
template <class S, int... E>
constexpr quantity<S, E...> operator*(quantity<S, E...> a, double b) {
    return {a.value * b};
}
template <class S, int... E>
constexpr quantity<S, E...> operator/(quantity<S, E...> a, double b) {
    return {a.value / b};
}
template <class S, int... E>
constexpr quantity<S, -E...> operator/(double a, quantity<S, E...> b) {
    return {a / b.value};
}

}  // namespace units
