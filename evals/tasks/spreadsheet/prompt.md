I want spreadsheet-style structs: some members carry a formula over the other members of the same struct, and one call recomputes them all. The formulas are parsed at compile time and evaluated in the order their dependencies need, whatever the declaration order is. A misspelled name, a syntax error or a cycle should be caught at compile time, and the same check should give me the evaluation order as text.

Everything goes in `namespace calc`:
- `calc::formula`: an annotation type, used as `[[=calc::formula("price * qty")]]` on a non-static data member. A member with this annotation is a computed member. Every other non-static data member is an input.
- `template <class T> constexpr T& calc::recalc(T& row)`: computes every computed member of `row` in evaluation order and returns `row`. It must also work during constant evaluation, as `sample` in the test shows.
- `consteval std::string_view calc::check(std::meta::info type)` (`constexpr` is fine too): the evaluation order of the struct `type` as text, or the first problem, as described below.

Formulas:
- A formula is an expression with this grammar, where `{ }` means zero or more times:
  - expression: term `{` (`+` or `-`) term `}`
  - term: factor `{` (`*` or `/`) factor `}`
  - factor: `-` factor, or `(` expression `)`, or a number, or a name
- The binary operators are left associative. `-x` means `0 - x`. Spaces may appear between tokens and are ignored.
- A number is one or more digits, optionally followed by `.` and one or more digits. A name is lowercase letters, digits and underscores and does not start with a digit. A name refers to the non-static data member of the same struct with that name, input or computed.
- Formulas compute in `double`. A member read by a formula is converted to `double`. The result is converted to the type of the computed member like `static_cast` does, so `7.5` stored in an `int` member is `7`.
- A formula reads the current value of each member. For a computed member, that is the value that this `recalc` call already stored in it.

Evaluation order and `check`:
- The order comes from this walk: visit every member in declaration order. Visiting an input member, or a computed member whose visit has ended, does nothing. Otherwise the formula of the member is parsed, then each member named in it is visited in the order the names appear in the text, and then the member's visit ends and it is added to the end of the evaluation order.
- The first problem found on this walk is reported, and the walk stops there. The formula is parsed from left to right and its first error is the one reported.
- On success, `check` returns `ok:` followed by a space and the name of each computed member, in evaluation order. So a struct whose computed members are `x` and then `y` gives `ok: x y`.
- A formula error gives `formula of '<member>': <reason>`, where `<member>` is the computed member that owns the formula and `<reason>` is one of these:
  - `unknown name '<name>'`: a name that is not a non-static data member of the struct.
  - `expected ')'`: a `(` expression that is not followed by `)`.
  - `expected a name or a number`: a factor is needed, but the text there does not start with `-`, `(`, a digit, a lowercase letter or `_`, or the text has ended.
  - `unexpected '<rest>'`: text is left after the whole expression. `<rest>` is that text, starting at its first character that is not a space.
- Visiting a computed member whose visit has started and not ended is a cycle. It gives `formula cycle: ` followed by the names of the members on the current visit path, starting with the member that is reached again, joined by ` -> `, and then ` -> ` and that member's name again.
- `recalc<T>` for a struct whose `check` does not start with `ok:` must not compile.

The annotation spellings must compile exactly as they appear in the test.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>

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

struct Grid {
    int w = 0;
    int h = 0;
    [[=calc::formula("w*h/2")]] int half = 0;  // computed as a double, then converted
    [[=calc::formula("half - (w - h) * 2.5")]] double mixed = 0;
};

struct Loop {
    [[=calc::formula("b + 1")]] double a;
    [[=calc::formula("c * 2")]] double b;
    [[=calc::formula("a")]] double c;
};

struct Typo {
    double width;
    [[=calc::formula("widht * 2")]] double twice;
};

struct Paren {
    double x;
    [[=calc::formula("(x + 1")]] double y;
};

constexpr Invoice sample = [] {
    Invoice i{.price = 20, .qty = 3, .discount = 10};
    return calc::recalc(i);
}();

int main() {
    Invoice inv{.price = 20, .qty = 3, .discount = 10};
    calc::recalc(inv);
    std::println("gross {} rebate {} net {} tax {} total {} per item {}", inv.gross, inv.rebate, inv.net, inv.tax, inv.total,
                 inv.per_item);
    std::println("{}", calc::check(^^Invoice));
    inv.qty = 10;
    std::println("with qty 10 the total is {}", calc::recalc(inv).total);
    std::println("at compile time: total {} per item {}", sample.total, sample.per_item);

    Grid g{.w = 3, .h = 5};
    calc::recalc(g);
    std::println("half {} mixed {}", g.half, g.mixed);
    std::println("{}", calc::check(^^Grid));

    std::println("{}", calc::check(^^Loop));
    std::println("{}", calc::check(^^Typo));
    std::println("{}", calc::check(^^Paren));
}
```

It must print exactly:

```
gross 60 rebate 6 net 54 tax 13.5 total 67.5 per item 22.5
ok: gross rebate net tax total per_item
with qty 10 the total is 225
at compile time: total 67.5 per item 22.5
half 7 mixed 12
ok: half mixed
formula cycle: a -> b -> c -> a
formula of 'twice': unknown name 'widht'
formula of 'y': expected ')'
```

Write the complete header to `solution.hpp` in the current directory.
