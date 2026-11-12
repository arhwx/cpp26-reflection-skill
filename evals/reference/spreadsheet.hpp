#pragma once

#include <algorithm>
#include <meta>
#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace calc {
using std::meta::info;

struct formula {  // the annotation
    const char* text;
    consteval formula(std::string_view t) : text(std::define_static_string(t)) {}
};

// expression nodes. a parsed formula is a type made of these, built with substitute
template <double V>
struct lit { static constexpr double eval(const auto&) { return V; } };
template <info Member>
struct ref { static constexpr double eval(const auto& row) { return row.[:Member:]; } };
template <char Op, class L, class R>
struct bin {
    static constexpr double eval(const auto& row) {
        double a = L::eval(row), b = R::eval(row);
        return Op == '+' ? a + b : Op == '-' ? a - b : Op == '*' ? a * b : a / b;
    }
};

consteval std::vector<info> fields(info type) { return nonstatic_data_members_of(type, std::meta::access_context::unprivileged()); }
consteval const char* text_of(info member) {  // the formula of a member, or null for an input
    for (info a : annotations_of(member))
        if (remove_const(type_of(a)) == ^^formula) return extract<formula>(a).text;
    return nullptr;
}

struct parser {  // recursive descent; level 0 is + and -, level 1 is * and /, level 2 is an atom
    info type, owner;
    std::string_view s;
    std::vector<info> reads{};  // members the formula depends on
    info root{};                // the result: a type such as bin<'*', ref<^^T::price>, ref<^^T::qty>>

    [[noreturn]] consteval void fail(std::string what) {
        throw std::meta::exception("formula of '" + std::string(identifier_of(owner)) + "': " + what, ^^parser);
    }
    consteval bool eat(char c) {
        while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
        if (s.empty() || s.front() != c) return false;
        s.remove_prefix(1);
        return true;
    }
    consteval info node(char op, info l, info r) { return substitute(^^bin, {std::meta::reflect_constant(op), l, r}); }
    consteval info number(double v) { return substitute(^^lit, {std::meta::reflect_constant(v)}); }
    consteval info atom() {
        if (eat('-')) return node('-', number(0), atom());
        if (eat('(')) {
            info e = expr(0);
            if (!eat(')')) fail("expected ')'");
            return e;
        }
        auto digit = [](char c) { return c >= '0' && c <= '9'; };
        auto letter = [](char c) { return c == '_' || (c >= 'a' && c <= 'z'); };
        auto take = [&](auto pred) {  // the longest prefix of s whose characters satisfy pred
            std::size_t n = 0;
            while (n < s.size() && pred(s[n])) ++n;
            std::string_view t = s.substr(0, n);
            s.remove_prefix(n);
            return t;
        };
        if (!s.empty() && digit(s.front())) {
            double v = 0, scale = 1;
            for (char c : take(digit)) v = v * 10 + (c - '0');
            if (s.size() > 1 && s[0] == '.' && digit(s[1])) {
                s.remove_prefix(1);
                for (char c : take(digit)) v = v * 10 + (c - '0'), scale *= 10;
            }
            return number(v / scale);
        }
        if (s.empty() || !letter(s.front())) fail("expected a name or a number");
        std::string_view token = take([&](char c) { return letter(c) || digit(c); });
        for (info m : fields(type))
            if (identifier_of(m) == token) return reads.push_back(m), substitute(^^ref, {std::meta::reflect_constant(m)});
        fail("unknown name '" + std::string(token) + "'");
    }
    consteval info expr(int level) {
        if (level == 2) return atom();
        for (info l = expr(level + 1);;) {
            char op = eat("+*"[level]) ? "+*"[level] : eat("-/"[level]) ? "-/"[level] : 0;
            if (!op) return l;
            l = node(op, l, expr(level + 1));
        }
    }
};
consteval parser compile(info type, info member) {
    parser p{type, member, text_of(member)};
    p.root = p.expr(0);
    if (!p.s.empty()) p.fail("unexpected '" + std::string(p.s) + "'");
    return p;
}

// depth-first topological order of the computed members
consteval void visit(info type, info m, std::vector<info>& path, std::vector<info>& order) {
    if (!text_of(m) || std::ranges::contains(order, m)) return;
    if (auto it = std::ranges::find(path, m); it != path.end()) {
        std::string cycle = "formula cycle: ";
        for (; it != path.end(); ++it) cycle += std::string(identifier_of(*it)) + " -> ";
        throw std::meta::exception(cycle + std::string(identifier_of(m)), ^^visit);
    }
    path.push_back(m);
    for (info dep : compile(type, m).reads) visit(type, dep, path, order);
    path.pop_back();
    order.push_back(m);
}
consteval std::vector<info> schedule(info type) {
    std::vector<info> path, order;
    for (info m : fields(type)) visit(type, m, path, order);
    return order;
}
consteval std::string_view check(info type) {  // "ok: " and the evaluation order, or the reason for rejection
    try {
        std::string names = "ok:";
        for (info m : schedule(type)) names += " " + std::string(identifier_of(m));
        return std::define_static_string(names);
    } catch (const std::meta::exception& e) {
        return std::define_static_string(std::string_view(e.what()));
    }
}

template <class T>
constexpr T& recalc(T& row) {
    static_assert(check(^^T).starts_with("ok:"), check(^^T));
    if constexpr (check(^^T).starts_with("ok:")) {  // a bad formula stops at the assertion above, with that one error
        template for (constexpr info m : std::define_static_array(schedule(^^T))) {
            using expr = [:compile(^^T, m).root:];
            row.[:m:] = static_cast<typename [:type_of(m):]>(expr::eval(row));
        }
    }
    return row;
}
}  // namespace calc

