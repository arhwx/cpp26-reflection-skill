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
