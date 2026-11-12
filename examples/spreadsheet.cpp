// spreadsheet: a struct whose members carry formulas. the formulas are parsed at compile time, ordered by
// their dependencies and compiled to plain member accesses. a cycle or a misspelled name is a compile error
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
        std::size_t n = 0;
        auto word = [&](char c) { return c == '_' || c == '.' || (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z'); };
        while (n < s.size() && word(s[n])) ++n;
        std::string_view token = s.substr(0, n);
        s.remove_prefix(n);
        if (token.empty()) fail("expected a name or a number");
        if (token[0] >= '0' && token[0] <= '9') {
            double v = 0, scale = 0;
            for (char c : token)
                if (c == '.') scale = 1;
                else v = v * 10 + (c - '0'), scale *= 10;
            return number(scale > 0 ? v / scale : v);
        }
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

// ---- user code
struct Invoice {
    double price = 0;
    int qty = 0;
    double discount = 0;  // percent
    [[=calc::formula("price * qty")]] double gross = 0;
    [[=calc::formula("net * 0.25")]] double tax = 0;  // reads a member declared later
    [[=calc::formula("gross * discount / 100")]] double rebate = 0;
    [[=calc::formula("gross - rebate")]] double net = 0;
    [[=calc::formula("net + tax")]] double total = 0;
    [[=calc::formula("-(0 - total) / qty")]] double per_item = 0;
};
struct Loop {
    [[=calc::formula("b + 1")]] double a;
    [[=calc::formula("c * 2")]] double b;
    [[=calc::formula("a")]] double c;
};
struct Typo { double width; [[=calc::formula("widht * 2")]] double twice; };

constexpr Invoice sample = [] { Invoice i{.price = 20, .qty = 3, .discount = 10}; return calc::recalc(i); }();
static_assert(sample.total == 67.5 && sample.per_item == 22.5);  // the same code also runs in the compiler

int main() {
    Invoice inv{.price = 20, .qty = 3, .discount = 10};
    calc::recalc(inv);
    std::println("gross {} rebate {} net {} tax {} total {} per item {}", inv.gross, inv.rebate, inv.net, inv.tax, inv.total, inv.per_item);
    std::println("{}", calc::check(^^Invoice));
    inv.qty = 10;
    std::println("with qty 10 the total is {}", calc::recalc(inv).total);
    std::println("{}", calc::check(^^Loop));
    std::println("{}", calc::check(^^Typo));
}

// output:
// gross 60 rebate 6 net 54 tax 13.5 total 67.5 per item 22.5
// ok: gross rebate net tax total per_item
// with qty 10 the total is 225
// formula cycle: a -> b -> c -> a
// formula of 'twice': unknown name 'widht'
