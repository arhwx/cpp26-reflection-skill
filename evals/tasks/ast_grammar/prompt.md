For small languages I already write the AST as plain structs and variants. I want the parser, the grammar documentation and a printer to come from those same types, so they cannot drift apart: the members of a struct give a sequence, a variant gives an ordered choice, and the punctuation and keywords are written as annotations.

Everything goes in `namespace peg`:
- `peg::lit("text")`: annotation on an empty struct, as in `struct [[=peg::lit("+")]] Plus {};`. The struct stands for the token `text`.
- `peg::prefix("text")` and `peg::suffix("text")`: annotations on a data member. They add the token `text` right before or right after the member. A member may have several of each; they apply in the order they are written.
- `template <class T> consteval std::string_view peg::grammar()`: the grammar of `T` as EBNF text.
- `template <class T> std::optional<T> peg::parse(std::string_view text)`: parses the whole text as a `T`.
- `template <class T> std::string peg::to_source(const T& value)`: prints a value back as tokens.

The types that can appear, and what each one means:
- a struct with a `peg::lit` annotation: its token;
- any other struct: a *rule*, the sequence of its non-static data members in declaration order, each with its prefixes before and its suffixes after it;
- `std::variant<A, B, ...>`: an ordered choice between the alternatives;
- `std::vector<X>`: zero or more `X`;
- `std::optional<X>`: zero or one `X`;
- `std::unique_ptr<X>`: the same as `X` (used for recursion);
- `int`: an integer token, one or more decimal digits;
- `std::string`: an identifier token, a letter or `_` followed by letters, digits and `_`.
Annotations other than these three are not used. All members are public, and rule structs are default constructible.

Grammar text:
- One line per rule, each ending in `\n`: `Name = items ;` where `Name` is the struct's name and the items are separated by single spaces.
- A member contributes its prefixes, then its own item, then its suffixes. A token is written in double quotes, `"+"`. A rule is written as its name, `int` as `integer`, `std::string` as `identifier`, `std::vector<X>` as `{ X }`, `std::optional<X>` as `[ X ]`, a variant as `( A | B | C )`, and `std::unique_ptr<X>` as `X` would be. Prefixes and suffixes stay outside the braces and brackets, so an optional member with prefix `"["` and suffix `"]"` is `"[" [ Items ] "]"`.
- The rule of `T` comes first. After it come the other rules in the order in which their names first appear, reading the lines already written from top to bottom and each line from left to right. Every rule is listed once.

Parsing (PEG semantics):
- Before every token, spaces, tabs and newlines are skipped. A literal token matches exactly its characters, with no check of what follows, so `"let"` matches the start of `letter`. An integer token takes all following digits, and an identifier all following identifier characters. Test values fit in `int`.
- A sequence matches its items one after the other and fails if any fails.
- A choice tries its alternatives in order from the same position and takes the first that matches, without trying the others later.
- `std::vector` repeats its element as often as it matches and stops at the first failure, at the position where that failed attempt began. `std::optional` is empty when its element fails, again without consuming anything.
- `parse` succeeds when `T` matches and only whitespace is left after it; otherwise it returns no value.

`to_source` lists the tokens of the value in the order of the grammar and joins them with single spaces: literal tokens (from `lit` structs, prefixes and suffixes) as their text, integers in decimal, identifiers as they are, and nothing for an empty optional or vector. Prefixes and suffixes are printed even when the member they belong to is empty.

The annotation spellings must compile exactly as they appear in the test.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
Program = { Let } Expr ;
Let = "let" identifier "=" Expr ";" ;
Expr = Term { AddOp } ;
Term = Factor { MulOp } ;
AddOp = ( "+" | "-" ) Term ;
Factor = [ "-" ] ( Num | Call | Var | Group ) ;
MulOp = ( "*" | "/" ) Factor ;
Num = integer ;
Call = identifier "(" Expr ")" ;
Var = identifier ;
Group = "(" Expr ")" ;
--
Value = ( "true" | "false" | integer | List ) ;
List = "[" [ Items ] "]" ;
Items = Value { More } ;
More = "," Value ;
--
let x = 2 * ( 3 + 4 ) ; let y = sq ( x ) - - 1 ; y / 3 + x => 79
abs ( - 5 ) * 2 => 10
7 => 7
no parse: "1 +"
no parse: "let = 3; 1"
no parse: "2 * (3"
no parse: "f(x"
let a = 1 ; let b = a + a ; b * b - a => 3
letter + 1 => 1
let x = 4 ; x => 4
expr: ( 1 ) = 1
factor: - n
[ 1 , [ true , false ] , [ ] , 7 ] sum 8
[ ] sum 0
42 sum 42
no parse: [1,]
no parse: [true false]
no parse: truefalse
[ [ [ 3 ] ] , 4 ] sum 7
```

Write the complete header to `solution.hpp` in the current directory.
