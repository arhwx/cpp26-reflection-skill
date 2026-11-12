#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <meta>
#include <string>
#include <string_view>
#include <vector>

namespace sym {
using std::meta::info;

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

// expression nodes. v points at the values of the variables
template <double V> struct lit { static constexpr double eval(const double*) { return V; } };
template <std::size_t I> struct var { static constexpr double eval(const double* v) { return v[I]; } };
template <class L, class R> struct add { static constexpr double eval(const double* v) { return L::eval(v) + R::eval(v); } };
template <class L, class R> struct sub { static constexpr double eval(const double* v) { return L::eval(v) - R::eval(v); } };
template <class L, class R> struct mul { static constexpr double eval(const double* v) { return L::eval(v) * R::eval(v); } };

// a malformed formula or an unknown variable name stops constant evaluation
consteval void fail(const char* why) { throw why; }

consteval info num(double v) { return substitute(^^lit, {std::meta::reflect_constant(v)}); }
consteval double value(info e) { return extract<double>(template_arguments_of(e)[0]); }
consteval bool is_lit(info e) { return template_of(e) == ^^lit; }
consteval std::vector<info> fields(info type) {
    return nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

// builds op<l, r> and simplifies on the way. two terms are equal exactly when their types are the same
consteval info make(info op, info l, info r) {
    const bool plus = op == ^^add;  // kept apart from && because ^^add && x would parse as ^^(add&&) x
    const bool minus = op == ^^sub;
    const bool times = op == ^^mul;
    if (is_lit(l) && is_lit(r))
        return num(plus ? value(l) + value(r) : minus ? value(l) - value(r) : value(l) * value(r));
    if (times && (l == num(0) || r == num(0))) return num(0);
    if (times && l == num(1)) return r;
    if (times && r == num(1)) return l;
    if (plus && l == num(0)) return r;
    if (!times && r == num(0)) return l;
    if (minus && l == r) return num(0);
    if (plus && l == r) return make(^^mul, num(2), l);
    return substitute(op, {l, r});
}

// the derivative of e with respect to the variable x
consteval info diff(info e, info x) {
    info op = template_of(e);
    if (op == ^^lit) return num(0);
    if (op == ^^var) return num(e == x ? 1 : 0);
    std::vector<info> a = template_arguments_of(e);
    if (op == ^^mul)
        return make(^^add, make(^^mul, diff(a[0], x), a[1]), make(^^mul, a[0], diff(a[1], x)));
    return make(op, diff(a[0], x), diff(a[1], x));
}

struct parser {  // level 0 is + and -, level 1 is *, level 2 is a number, a variable or parentheses
    std::string_view s;
    std::vector<std::string_view> names{};  // variables in order of first use
    info root{};

    consteval void skip() {
        while (!s.empty() && s.front() == ' ') s.remove_prefix(1);
    }
    consteval bool eat(char c) {
        skip();
        if (s.empty() || s.front() != c) return false;
        s.remove_prefix(1);
        return true;
    }
    consteval info expr(int level = 0) {
        if (level == 2) {
            if (eat('(')) {
                info e = expr();
                if (!eat(')')) fail("expected ')'");
                return e;
            }
            skip();
            std::size_t n = 0;
            while (n < s.size() && ((s[n] >= '0' && s[n] <= '9') || (s[n] >= 'a' && s[n] <= 'z'))) ++n;
            std::string_view token = s.substr(0, n);
            s.remove_prefix(n);
            if (token.empty()) fail("expected a variable or a number");
            if (token[0] <= '9') {
                double v = 0;
                for (char c : token) {
                    if (c > '9') fail("a number must contain only digits");
                    v = v * 10 + (c - '0');
                }
                return num(v);
            }
            if (!std::ranges::contains(names, token)) names.push_back(token);
            std::size_t index = std::ranges::find(names, token) - names.begin();
            return substitute(^^var, {std::meta::reflect_constant(index)});
        }
        for (info l = expr(level + 1);;) {
            info op = level == 1 ? (eat('*') ? ^^mul : info{}) : eat('+') ? ^^add : eat('-') ? ^^sub : info{};
            if (op == info{}) return l;
            l = make(op, l, expr(level + 1));
        }
    }
};
consteval parser parse(std::string_view src) {
    parser p{src};
    p.root = p.expr();
    p.skip();
    if (!p.s.empty()) fail("unexpected text after the formula");
    return p;
}

consteval std::string show(info e, info args) {
    info op = template_of(e);
    std::vector<info> a = template_arguments_of(e);
    if (op == ^^var) return std::string(identifier_of(fields(args)[extract<std::size_t>(a[0])]));
    if (op != ^^lit)
        return "(" + show(a[0], args) + (op == ^^add ? " + " : op == ^^sub ? " - " : " * ") + show(a[1], args) + ")";
    double v = value(e);  // constants are whole numbers
    long long n = static_cast<long long>(v < 0 ? -v : v);
    std::string digits;
    for (; n > 0 || digits.empty(); n /= 10) digits.insert(digits.begin(), char('0' + n % 10));
    return v < 0 ? "-" + digits : digits;
}
consteval info variable(info args, std::string_view name) {
    std::size_t i = 0;
    for (info m : fields(args)) {
        if (identifier_of(m) == name) return substitute(^^var, {std::meta::reflect_constant(i)});
        ++i;
    }
    fail("no variable with this name");
    return {};
}

template <fixed_string Src>
struct args_of {
    struct type;  // one double per variable, named after it
    consteval {
        std::vector<info> specs;
        for (std::string_view name : parse(Src.view()).names) specs.push_back(data_member_spec(^^double, {.name = name}));
        define_aggregate(^^type, specs);
    }
};

template <info Expr, class Args>
struct function {
    using args = Args;
    static constexpr auto members = std::define_static_array(fields(^^Args));
    static constexpr std::string_view text = std::define_static_string(show(Expr, ^^Args));
    template <fixed_string Name>
    using d = function<diff(Expr, variable(^^Args, Name.view())), Args>;  // a derivative is a function again

    static constexpr double eval(const Args& a) {
        std::array<double, members.size()> v{};
        std::size_t i = 0;
        template for (constexpr info m : members) v[i++] = a.[:m:];
        using expr = [:Expr:];
        return expr::eval(v.data());
    }
    static constexpr Args grad(const Args& a) {  // every partial derivative, under the name of its variable
        Args g{};
        template for (constexpr info m : members)
            g.[:m:] = function<diff(Expr, variable(^^Args, identifier_of(m))), Args>::eval(a);
        return g;
    }
};
template <fixed_string Src>
using fn = function<parse(Src.view()).root, typename args_of<Src>::type>;
}  // namespace sym
