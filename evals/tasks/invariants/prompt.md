I want class invariants written as text on the class, like `[[=inv::invariant("low <= high")]]`. The text is parsed at compile time against the members of the class, so a typo in a member name or a syntax error is found by the compiler. A `checked<T>` wrapper then keeps a value whose invariants hold: every change is checked, and a change that breaks an invariant is undone and reported.

Everything goes in `namespace inv`:
- `inv::invariant("text")`: an annotation on a class, used as `[[=inv::invariant("low <= high")]]`. A class can have several. Its invariants are the texts of these annotations in the order they are written. A class without them has no invariants.
- `inv::violation`: an exception class derived from `std::logic_error`.
- `consteval std::string_view inv::check(std::meta::info type)`: `ok` if every invariant of the class `type` is valid, otherwise a message (see below).
- `template <class T> constexpr bool inv::holds(const T& value)`: whether every invariant of `T` holds for `value`. It must work in constant expressions.
- `template <class T> std::vector<std::string> inv::broken(const T& value)`: the texts of the invariants of `T` that do not hold for `value`, in invariant order.
- `template <class T> class inv::checked`:
  - `explicit checked(T value)`: stores `value`. If an invariant does not hold, it throws `violation`.
  - `const T& get() const` and `const T* operator->() const`: read access. There is no other way to reach the stored value.
  - `template </* name */ Name, class V> void set(V&& v)`, called as `set<"high">(20)`: the template argument is a string literal naming a public non-static data member of `T`. The call assigns `std::forward<V>(v)` to that member, then checks. If `T` has no public non-static data member with that name, the call must not be viable, so that a requires-expression containing it is `false`.
  - `template <class F> void modify(F f)`: calls `f` with a `T&` that it may change in any way, then checks once, after `f` returns. States in between are not checked.
  - Checking after `set` or `modify`: if every invariant holds, the change is kept. Otherwise the stored value is exactly what it was before the call, and `violation` is thrown.
  - The message of a thrown `violation` (its `what()`) describes the first invariant, in invariant order, that does not hold.
- Using `holds`, `broken` or `checked` with a class that `check` rejects must not compile, but the test does not do that.

Invariant text. Tokens may be separated by spaces, which are ignored; there are no spaces inside a token. The grammar, where `{ }` means zero or more times and `[ ]` means optional:
- disjunction: conjunction `{` `||` conjunction `}`
- conjunction: comparison `{` `&&` comparison `}`
- comparison: sum `[` (`==` or `!=` or `<` or `<=` or `>` or `>=`) sum `]`. So `a < b <= c` is not valid.
- sum: product `{` (`+` or `-`) product `}`
- product: factor `{` (`*` or `/`) factor `}`
- factor: `!` factor, or `-` factor, or `(` disjunction `)`, or a number, or a name
- A number is one or more digits, optionally followed by `.` and one or more digits. A name is a letter or `_` followed by letters, digits and `_`. A name refers to the public non-static data member of the class with that name; private and protected members cannot be named.
- The whole text must be one disjunction. Binary operators are left associative.

Evaluation: every value is a `double`. A member is converted with `static_cast<double>`, so a `bool` is 0 or 1. `-x` negates, `!x` is 1 when `x` is 0 and 0 otherwise. `+ - * /` are the `double` operations. A comparison is 1 when true and 0 when false. `a && b` is 1 when both are non-zero, `a || b` is 1 when either is. An invariant holds when its value is not 0.

`check` messages. `<T>` is the unqualified name of the class.
- The invariants are examined in order, and the first one with a problem is reported.
- Its text is read from left to right according to the grammar, and the first problem met is the one reported. A name where the grammar allows a name, but which is not a public non-static data member, gives `<T>: unknown name '<name>' in '<text>'`. Any other way the text does not fit the grammar, including text left over after a complete disjunction, gives `<T>: cannot parse '<text>'`.

`violation` messages: `<T>: '<text>' is broken (<name> = <value>, <name> = <value>)`, listing each member named in the text once, in the order of their first appearance in the text, with the value of the member in the value that was checked and rejected (not the restored one), written as `std::format("{}", member)` writes it. If the text names no member, the part from ` (` on is left out.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>
#include <vector>

struct [[=inv::invariant("low <= high")]] [[=inv::invariant("step > 0 && step <= high - low")]] Range {
    int low;
    int high;
    int step;
};

struct [[=inv::invariant("!closed || balance == 0")]] [[=inv::invariant("balance >= -(limit * 1.5)")]] Account {
    bool closed;
    double balance;
    int limit;
};

struct Point {
    int x;
    int y;
};

// rejected invariants
struct [[=inv::invariant("lo <= hi")]] Typo { int low, high; };
struct [[=inv::invariant("a < b")]] [[=inv::invariant("a < b <= c")]] Chain { int a, b, c; };
struct [[=inv::invariant("(a + b > c")]] Open { int a, b, c; };
struct [[=inv::invariant("a = 1 || zz > 0")]] Assign { int a; };
struct [[=inv::invariant("zz > 0 || a = 1")]] Unknown { int a; };
struct [[=inv::invariant("a > 1.")]] Dot { int a; };
class [[=inv::invariant("x < y")]] Secret {
    int x = 1;

public:
    int y = 2;
};

static_assert(inv::holds(Range{0, 10, 2}));
static_assert(!inv::holds(Range{0, 10, 20}));
static_assert(inv::holds(Point{-1, -1}));

template <class C>
concept can_set_low = requires(C& c) { c.template set<"low">(1); };
template <class C>
concept can_set_lo = requires(C& c) { c.template set<"lo">(1); };
template <class C>
concept writable = requires(C& c) { c->low = 1; };

void show(const inv::checked<Range>& r) { std::println("  range {} {} {}", r->low, r->high, r->step); }
void show(const inv::checked<Account>& a) {
    std::println("  account closed={} balance={} limit={}", a->closed, a->balance, a.get().limit);
}

void list(const std::vector<std::string>& texts) {
    std::print("  broken: {}", texts.size());
    for (const std::string& t : texts) std::print(" [{}]", t);
    std::println();
}

template <class F>
void attempt(const char* what, F f) {
    try {
        f();
        std::println("{}: ok", what);
    } catch (const inv::violation& e) {
        std::println("{}: {}", what, e.what());
    }
}

int main() {
    std::println("Range: {}", inv::check(^^Range));
    std::println("Account: {}", inv::check(^^Account));
    std::println("Point: {}", inv::check(^^Point));
    std::println("{}", inv::check(^^Typo));
    std::println("{}", inv::check(^^Chain));
    std::println("{}", inv::check(^^Open));
    std::println("{}", inv::check(^^Assign));
    std::println("{}", inv::check(^^Unknown));
    std::println("{}", inv::check(^^Dot));
    std::println("{}", inv::check(^^Secret));
    std::println("set low: {}, set lo: {}, write through ->: {}", can_set_low<inv::checked<Range>>,
                 can_set_lo<inv::checked<Range>>, writable<inv::checked<Range>>);

    inv::checked<Range> r(Range{0, 10, 2});
    attempt("high = 20", [&] { r.set<"high">(20); });
    attempt("low = 25", [&] { r.set<"low">(25); });
    attempt("step = 0", [&] { r.set<"step">(0); });
    show(r);
    attempt("move to 30..40", [&] { r.modify([](Range& x) { x.low = 30; x.high = 40; }); });
    attempt("low 35, step 10", [&] { r.modify([](Range& x) { x.low = 35; x.step = 10; }); });
    show(r);
    attempt("construct 3..1", [] { inv::checked<Range> bad(Range{3, 1, 1}); });
    list(inv::broken(Range{3, 1, 5}));
    list(inv::broken(Range{0, 10, 2}));

    inv::checked<Account> acc(Account{false, -10.5, 10});
    attempt("close", [&] { acc.set<"closed">(true); });
    attempt("balance -16", [&] { acc.set<"balance">(-16); });
    attempt("limit 20", [&] { acc.set<"limit">(20); });
    attempt("balance -16 again", [&] { acc.set<"balance">(-16); });
    show(acc);
    attempt("settle and close", [&] { acc.modify([](Account& a) { a.balance = 0; a.closed = true; }); });
    attempt("overdraw closed", [&] { acc.set<"balance">(-0.25); });
    show(acc);
    list(inv::broken(Account{true, -40, 20}));

    inv::checked<Point> p(Point{1, 2});
    attempt("point x = -5", [&] { p.set<"x">(-5); });
    std::println("  point {} {}", p->x, p->y);
}
```

It must print exactly:

```
Range: ok
Account: ok
Point: ok
Typo: unknown name 'lo' in 'lo <= hi'
Chain: cannot parse 'a < b <= c'
Open: cannot parse '(a + b > c'
Assign: cannot parse 'a = 1 || zz > 0'
Unknown: unknown name 'zz' in 'zz > 0 || a = 1'
Dot: cannot parse 'a > 1.'
Secret: unknown name 'x' in 'x < y'
set low: true, set lo: false, write through ->: false
high = 20: ok
low = 25: Range: 'low <= high' is broken (low = 25, high = 20)
step = 0: Range: 'step > 0 && step <= high - low' is broken (step = 0, high = 20, low = 0)
  range 0 20 2
move to 30..40: ok
low 35, step 10: Range: 'step > 0 && step <= high - low' is broken (step = 10, high = 40, low = 35)
  range 30 40 2
construct 3..1: Range: 'low <= high' is broken (low = 3, high = 1)
  broken: 2 [low <= high] [step > 0 && step <= high - low]
  broken: 0
close: Account: '!closed || balance == 0' is broken (closed = true, balance = -10.5)
balance -16: Account: 'balance >= -(limit * 1.5)' is broken (balance = -16, limit = 10)
limit 20: ok
balance -16 again: ok
  account closed=false balance=-16 limit=20
settle and close: ok
overdraw closed: Account: '!closed || balance == 0' is broken (closed = true, balance = -0.25)
  account closed=true balance=0 limit=20
  broken: 2 [!closed || balance == 0] [balance >= -(limit * 1.5)]
point x = -5: ok
  point -5 2
```

Write the complete header to `solution.hpp` in the current directory.
