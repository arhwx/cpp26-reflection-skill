// contracts: class invariants written as text on the class and parsed at compile time. checked<T> checks
// every change and undoes the ones that break an invariant, and a typo in an invariant is a compile error
#include <algorithm>
#include <format>
#include <functional>
#include <meta>
#include <print>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inv {
using std::meta::info;

struct invariant {  // the annotation
    const char* text;
    consteval invariant(std::string_view t) : text(std::define_static_string(t)) {}
};
struct violation : std::logic_error { using std::logic_error::logic_error; };

template <std::size_t N>
struct name {  // a string literal as a template argument, as in set<"low">
    char s[N]{};
    consteval name(const char (&str)[N]) { std::ranges::copy(str, s); }
};

consteval info find_field(info type, std::string_view word) {  // the public member called word, or a null reflection
    for (info m : nonstatic_data_members_of(type, std::meta::access_context::unprivileged()))
        if (identifier_of(m) == word) return m;
    return {};
}
consteval std::vector<const char*> texts(info type) {  // the invariants of a class, in order
    std::vector<const char*> out;
    for (info a : annotations_of(type))
        if (remove_const(type_of(a)) == ^^invariant) out.push_back(extract<invariant>(a).text);
    return out;
}

// a parsed invariant is a type made of these nodes: "low <= high" is call<std::less_equal<>, field<^^Range::low>, field<^^Range::high>>
template <double V>
struct number { static constexpr double eval(const auto&) { return V; } };
template <info M>
struct field { static constexpr double eval(const auto& obj) { return static_cast<double>(obj.[:M:]); } };
template <class Op, class... Args>
struct call { static constexpr double eval(const auto& obj) { return Op{}(Args::eval(obj)...); } };

struct op { int level; std::string_view token; info fn; };
constexpr op ops[] = {  // the binary operators, level 0 binds loosest
    {0, "||", ^^std::logical_or<>}, {1, "&&", ^^std::logical_and<>},
    {2, "==", ^^std::equal_to<>}, {2, "!=", ^^std::not_equal_to<>}, {2, "<=", ^^std::less_equal<>},
    {2, ">=", ^^std::greater_equal<>}, {2, "<", ^^std::less<>}, {2, ">", ^^std::greater<>},
    {3, "+", ^^std::plus<>}, {3, "-", ^^std::minus<>}, {4, "*", ^^std::multiplies<>}, {4, "/", ^^std::divides<>},
};
struct parser {  // recursive descent, one call per level
    info type;
    std::string_view text, s;   // all of the text, and the part not read yet
    std::vector<info> names{};  // the members it reads, in order of first use
    std::string error{};        // the first problem, if any
    info root{};

    consteval info fail(std::string why) {  // keeps the first problem, stops reading and returns a stand-in
        if (error.empty()) error = std::string(identifier_of(type)) + ": " + why + " in '" + std::string(text) + "'";
        s = {};
        return substitute(^^number, {std::meta::reflect_constant(0.0)});
    }
    consteval info unexpected() { return fail(s.empty() ? "unexpected end" : "unexpected '" + std::string(s) + "'"); }
    consteval bool eat(std::string_view token) {  // skips spaces, then takes token if it comes next
        while (s.starts_with(' ')) s.remove_prefix(1);
        if (!s.starts_with(token)) return false;
        s.remove_prefix(token.size());
        return true;
    }
    consteval const op* next(int level) {  // takes an operator of this level if one comes next
        for (const op& o : ops)
            if (o.level == level && eat(o.token)) return &o;
        return nullptr;
    }
    consteval info expr(int level) {
        if (level == 5) return factor();
        info l = expr(level + 1);
        while (const op* o = next(level)) {
            l = substitute(^^call, {o->fn, l, expr(level + 1)});
            if (level == 2) break;  // comparisons do not chain, so "a < b <= c" is rejected
        }
        return l;
    }
    consteval info factor() {
        if (eat("!")) return substitute(^^call, {^^std::logical_not<>, factor()});
        if (eat("-")) return substitute(^^call, {^^std::negate<>, factor()});
        if (eat("(")) { info e = expr(0); return eat(")") ? e : unexpected(); }
        auto digit = [](char c) { return c >= '0' && c <= '9'; };
        auto letter = [](char c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); };
        if (!s.empty() && digit(s[0])) {  // a whole number
            double v = 0;
            for (; !s.empty() && digit(s[0]); s.remove_prefix(1)) v = v * 10 + (s[0] - '0');
            return substitute(^^number, {std::meta::reflect_constant(v)});
        }
        std::size_t n = 0;
        while (n < s.size() && (letter(s[n]) || digit(s[n]))) ++n;
        if (n == 0) return unexpected();
        std::string_view word = s.substr(0, n);
        info m = find_field(type, word);
        if (m == info{}) return fail("unknown name '" + std::string(word) + "'");
        s.remove_prefix(n);
        if (!std::ranges::contains(names, m)) names.push_back(m);
        return substitute(^^field, {std::meta::reflect_constant(m)});
    }
};
consteval parser compile(info type, std::string_view text) {
    parser p{type, text, text};
    p.root = p.expr(0);
    if (p.eat(""); !p.s.empty()) p.unexpected();  // after trailing spaces, nothing may be left
    return p;
}
consteval std::string_view check(info type) {  // "ok", or why an invariant of the class is rejected
    for (const char* text : texts(type))
        if (std::string error = compile(type, text).error; !error.empty()) return std::define_static_string(error);
    return "ok";
}

template <class T>
constexpr bool holds(const T& value, bool or_throw = false) {  // or_throw: throw a violation for the first broken one
    static_assert(check(^^T) == "ok", check(^^T));
    template for (constexpr const char* text : std::define_static_array(texts(^^T))) {
        using expr = [:compile(^^T, text).root:];
        if (expr::eval(value) == 0) {
            if (!or_throw) return false;
            std::string values;
            template for (constexpr info m : std::define_static_array(compile(^^T, text).names))
                values += std::format("{}{} = {}", values.empty() ? "" : ", ", identifier_of(m), value.[:m:]);
            throw violation(std::format("{}: '{}' is broken ({})", identifier_of(^^T), text, values));
        }
    }
    return true;
}

template <class T>
class checked {  // a T whose invariants hold. reading is free, every change is checked
    T value;

public:
    explicit checked(T v) : value(std::move(v)) { holds(value, true); }
    const T* operator->() const { return &value; }
    template <name N, class V>
        requires(find_field(^^T, N.s) != info{})
    void set(V&& v) {
        modify([&](T& x) { x.[:find_field(^^T, N.s):] = std::forward<V>(v); });
    }
    template <class F>
    void modify(F f) {  // f may break invariants on the way, only the result is checked
        T next = value;
        f(next);
        holds(next, true);  // on a throw, value is left as it was
        value = std::move(next);
    }
};
}  // namespace inv

// ---- user code. eiffel: invariant low <= high, d: invariant { assert(low <= high); }
struct [[=inv::invariant("low <= high")]] [[=inv::invariant("step > 0 && step <= high - low")]] Range {
    int low, high, step;
};
struct [[=inv::invariant("lo <= hi")]] Typo { int low, high; };
struct [[=inv::invariant("0 <= pct <= 100")]] Percent { int pct; };

static_assert(inv::holds(Range{0, 10, 2}) && !inv::holds(Range{0, 10, 20}));  // the checks also run in the compiler
template <class T, inv::name N>
concept settable = requires(inv::checked<T>& c) { c.template set<N>(0); };
static_assert(settable<Range, "high"> && !settable<Range, "hihg">);  // set takes only real member names
// r->low = 5;                    does not compile: reads only, changes go through set and modify
// inv::checked<Typo> t({0, 1});  does not compile: Typo: unknown name 'lo' in 'lo <= hi'

void attempt(const char* what, auto change) {
    try { change(); std::println("{}: ok", what); }
    catch (const inv::violation& e) { std::println("{}: {}", what, e.what()); }
}

int main() {
    inv::checked<Range> r(Range{.low = 0, .high = 10, .step = 2});
    attempt("high = 20", [&] { r.set<"high">(20); });
    attempt("low = 25", [&] { r.set<"low">(25); });
    attempt("step = 0", [&] { r.set<"step">(0); });
    std::println("range {}..{} step {}", r->low, r->high, r->step);  // the failed changes were undone
    attempt("move to 30..40", [&] { r.modify([](Range& x) { x.low = 30; x.high = 40; }); });  // low = 30 alone would break low <= high
    std::println("range {}..{} step {}", r->low, r->high, r->step);
    std::println("{}", inv::check(^^Typo));
    std::println("{}", inv::check(^^Percent));
}

// output:
// high = 20: ok
// low = 25: Range: 'low <= high' is broken (low = 25, high = 20)
// step = 0: Range: 'step > 0 && step <= high - low' is broken (step = 0, high = 20, low = 0)
// range 0..20 step 2
// move to 30..40: ok
// range 30..40 step 2
// Typo: unknown name 'lo' in 'lo <= hi'
// Percent: unexpected '<= 100' in '0 <= pct <= 100'
