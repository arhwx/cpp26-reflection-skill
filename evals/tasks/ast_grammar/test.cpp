#include "solution.hpp"

#include <map>
#include <memory>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

// a small calculator language
struct [[=peg::lit("+")]] Plus {};
struct [[=peg::lit("-")]] Minus {};
struct [[=peg::lit("*")]] Times {};
struct [[=peg::lit("/")]] Over {};

struct Expr;
struct Num {
    int value;
};
struct Var {
    std::string name;
};
struct Call {
    std::string fn;
    [[=peg::prefix("(")]] [[=peg::suffix(")")]] std::unique_ptr<Expr> arg;
};
struct Group {
    [[=peg::prefix("(")]] [[=peg::suffix(")")]] std::unique_ptr<Expr> inner;
};
struct Factor {
    std::optional<Minus> neg;
    std::variant<Num, Call, Var, Group> value;
};
struct MulOp {
    std::variant<Times, Over> op;
    Factor rhs;
};
struct Term {
    Factor first;
    std::vector<MulOp> rest;
};
struct AddOp {
    std::variant<Plus, Minus> op;
    Term rhs;
};
struct Expr {
    Term first;
    std::vector<AddOp> rest;
};
struct Let {
    [[=peg::prefix("let")]] std::string name;
    [[=peg::prefix("=")]] [[=peg::suffix(";")]] Expr value;
};
struct Program {
    std::vector<Let> lets;
    Expr result;
};

using Env = std::map<std::string, int>;
int eval(const Expr& e, const Env& env);

int eval(const Factor& f, const Env& env) {
    int v = std::visit([&](const auto& x) -> int {
        using X = std::remove_cvref_t<decltype(x)>;
        if constexpr (std::is_same_v<X, Num>) return x.value;
        else if constexpr (std::is_same_v<X, Var>) return env.contains(x.name) ? env.at(x.name) : 0;
        else if constexpr (std::is_same_v<X, Group>) return eval(*x.inner, env);
        else {
            // sq or abs
            int a = eval(*x.arg, env);
            return x.fn == "sq" ? a * a : a < 0 ? -a : a;
        }
    }, f.value);
    return f.neg ? -v : v;
}
int eval(const Term& t, const Env& env) {
    int v = eval(t.first, env);
    for (const MulOp& m : t.rest) v = m.op.index() == 0 ? v * eval(m.rhs, env) : v / eval(m.rhs, env);
    return v;
}
int eval(const Expr& e, const Env& env) {
    int v = eval(e.first, env);
    for (const AddOp& a : e.rest) v = a.op.index() == 0 ? v + eval(a.rhs, env) : v - eval(a.rhs, env);
    return v;
}

// nested lists of numbers and booleans
struct [[=peg::lit("true")]] Yes {};
struct [[=peg::lit("false")]] No {};
struct Value;
struct More {
    [[=peg::prefix(",")]] std::unique_ptr<Value> item;
};
struct Items {
    std::unique_ptr<Value> first;
    std::vector<More> more;
};
struct List {
    [[=peg::prefix("[")]] [[=peg::suffix("]")]] std::optional<Items> items;
};
struct Value {
    std::variant<Yes, No, int, List> v;
};

int sum(const Value& v) {
    if (const int* n = std::get_if<int>(&v.v)) return *n;
    const List* l = std::get_if<List>(&v.v);
    if (!l || !l->items) return 0;
    int s = sum(*l->items->first);
    for (const More& m : l->items->more) s += sum(*m.item);
    return s;
}

static_assert(peg::grammar<Value>().starts_with("Value = ( \"true\" | \"false\" | integer | List ) ;\n"));

int main() {
    std::print("{}", peg::grammar<Program>());
    std::println("--");
    std::print("{}", peg::grammar<Value>());
    std::println("--");

    for (std::string_view src : {"let x = 2 * (3 + 4);\nlet y = sq(x) - -1;\n  y / 3 + x", "abs (-5) * 2", "  7  ",
                                 "1 +", "let = 3; 1", "2 * (3", "f(x", "let a=1;let b=a+a;b*b-a", "letter + 1", "letx=4;x"}) {
        if (auto p = peg::parse<Program>(src)) {
            Env env;
            for (const Let& l : p->lets) env[l.name] = eval(l.value, env);
            std::println("{} => {}", peg::to_source(*p), eval(p->result, env));
        } else {
            std::println("no parse: {:?}", src);
        }
    }

    if (auto e = peg::parse<Expr>("(1)")) std::println("expr: {} = {}", peg::to_source(*e), eval(*e, {}));
    if (auto f = peg::parse<Factor>("-n")) std::println("factor: {}", peg::to_source(*f));

    for (std::string_view src : {"[1, [true, false], [], 7]", "[ ]", "42", "[1,]", "[true false]", "truefalse", "[[[3]],4]"}) {
        if (auto v = peg::parse<Value>(src)) std::println("{} sum {}", peg::to_source(*v), sum(*v));
        else std::println("no parse: {}", src);
    }
}
