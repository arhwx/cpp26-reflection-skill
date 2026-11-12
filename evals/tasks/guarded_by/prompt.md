Our team keeps writing comments like `// guarded by mutex` next to members, and nothing checks them. I want the guard written as an annotation that names the mutex member, and a `locked(obj)` call that locks the right mutexes and returns a view through which the guarded members are reachable only while the lock is held.

Everything goes in `namespace lock`:
- `lock::guarded_by(r)`: annotation on a non-static data member. `r` is a reflection of the mutex member that guards it, written in the class body as `[[=lock::guarded_by(^^mutex)]]` or `[[=lock::guarded_by(^^Bank::audit_mutex)]]`.
- `lock::locked(obj)`: takes a non-const lvalue of a class type `T` and returns a guard object `g`. Creating it locks every non-static data member of `T` that guards at least one member of `T`, each exactly once, in the declaration order of those mutex members. Destroying it unlocks them in the reverse order. Mutex members that guard nothing are not touched. `g` cannot be copied.
- `g->name` and `(*g).name` are the guarded member `name` of `obj` itself, as an lvalue reference of the member's declared type: `decltype(g->total)` is `long&`. Only the members exposed by the view can be named this way. Naming any other member through `g`, such as an unguarded member or a mutex, must be ill-formed, so that a requires-expression doing it is `false`.
- `lock::locked<m>(obj)`, where `m` is a reflection of a member of `T`: the same, but it locks only `m` and exposes only the members guarded by `m`. It is viable only when `m` is a non-static data member of `T` that guards at least one member. Otherwise the call must not compile, and a requires-expression containing it is `false`.
- Both forms of `locked` are viable only when `lock::check<T>()` is `"ok"`. A class with no guarded members is fine: its view locks nothing and exposes nothing.
- Guarded members and mutexes may be private. `locked` locks and exposes them anyway, so a class can call `lock::locked(*this)` in its own member functions, as `Account` does.
- A mutex is any member whose type has `lock()` and `unlock()` callable on a non-const lvalue.
- Members may also carry annotations from other libraries, like `audit::note` in the test. Those must be ignored.

`lock::check<T>()` is a `consteval` function returning a `std::string_view`. It returns `"ok"` if every `guarded_by` annotation of `T` is valid. Otherwise it describes the first problem, going through the members of `T` in declaration order, as `<class>::<member>: <problem>`, where `<class>` is the unqualified name of `T` and `<member>` the name of the annotated member. For each annotated member these checks run in this order, and the first that fails gives the problem:
1. The member has more than one `guarded_by` annotation: `guarded more than once`.
2. The reflection does not name a non-static data member of `T` itself: `guard is not a data member of <class>`.
3. The type of that member is not a mutex as defined above: `guard cannot be locked`.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <mutex>
#include <print>
#include <string>
#include <type_traits>
#include <vector>

namespace audit {
struct note {
    int level;
};
}  // namespace audit

// a mutex that reports what happens to it
struct noisy_mutex {
    const char* name;
    bool held = false;
    void lock() {
        std::println("  lock {}", name);
        held = true;
    }
    void unlock() {
        std::println("  unlock {}", name);
        held = false;
    }
};

class Account {
public:
    explicit Account(long opening) {
        auto g = lock::locked(*this);
        g->balance = opening;
        g->history.push_back("open " + std::to_string(opening));
    }

    void deposit(long amount) {
        auto g = lock::locked(*this);
        g->balance += amount;
        g->history.push_back("deposit " + std::to_string(amount));
        std::println("  inside: balance {}, held {}", g->balance, mutex.held);
    }

    void report() {
        auto g = lock::locked(*this);
        for (const std::string& line : g->history) std::println("  {}", line);
    }

    noisy_mutex mutex{"account"};

private:
    [[=lock::guarded_by(^^mutex)]] long balance = 0;
    int id = 7;
    [[=audit::note{1}]] [[=lock::guarded_by(^^mutex)]] std::vector<std::string> history;
};

struct Bank {
    noisy_mutex ledger_mutex{"ledger"};
    noisy_mutex audit_mutex{"audit"};
    noisy_mutex spare_mutex{"spare"};
    [[=lock::guarded_by(^^Bank::audit_mutex)]] int audits = 0;
    [[=lock::guarded_by(^^Bank::ledger_mutex)]] long total = 0;
    std::string name = "central";
    [[=lock::guarded_by(^^Bank::ledger_mutex)]] int entries = 0;
};

struct Counter {
    std::mutex m;
    [[=lock::guarded_by(^^m)]] int hits = 0;
};

struct Plain {
    int a = 1;
};

struct Foreign {
    int x;
    [[=lock::guarded_by(^^Bank::ledger_mutex)]] int y;
};

struct NotLockable {
    int flag;
    noisy_mutex fine{"fine"};
    [[=lock::guarded_by(^^fine)]] int ok;
    [[=lock::guarded_by(^^flag)]] int bad;
};

struct Twice {
    noisy_mutex a{"a"};
    noisy_mutex b{"b"};
    [[=lock::guarded_by(^^a)]] [[=lock::guarded_by(^^b)]] int z;
};

template <class V>
concept sees_total = requires(V& v) { v->total; };
template <class V>
concept sees_audits = requires(V& v) { v->audits; };
template <class V>
concept sees_name = requires(V& v) { v->name; };

template <class T>
concept can_lock = requires(T& t) { lock::locked(t); };
template <class T>
concept can_lock_spare = requires(T& t) { lock::locked<^^Bank::spare_mutex>(t); };
template <class T>
concept can_lock_name = requires(T& t) { lock::locked<^^Bank::name>(t); };

int main() {
    std::println("open");
    Account acc(100);
    std::println("deposit");
    acc.deposit(25);
    std::println("after: held {}", acc.mutex.held);
    std::println("report");
    acc.report();

    Bank bank;
    std::println("whole bank");
    {
        auto g = lock::locked(bank);
        g->total += 500;
        g->entries += 1;
        g->audits += 1;
        static_assert(std::is_same_v<decltype(g->total), long&>);
        std::println("  sees total {}, audits {}, name {}", sees_total<decltype(g)>, sees_audits<decltype(g)>,
                     sees_name<decltype(g)>);
        std::println("  held: ledger {}, audit {}, spare {}", bank.ledger_mutex.held, bank.audit_mutex.held,
                     bank.spare_mutex.held);
    }
    std::println("audit only");
    {
        auto g = lock::locked<^^Bank::audit_mutex>(bank);
        g->audits += 1;
        std::println("  sees total {}, audits {}", sees_total<decltype(g)>, sees_audits<decltype(g)>);
        std::println("  held: ledger {}, audit {}", bank.ledger_mutex.held, bank.audit_mutex.held);
    }
    std::println("ledger only");
    {
        auto g = lock::locked<^^Bank::ledger_mutex>(bank);
        std::println("  total {}, entries {}", g->total, (*g).entries);
        std::println("  sees total {}, audits {}", sees_total<decltype(g)>, sees_audits<decltype(g)>);
        std::println("  copyable {}", std::is_copy_constructible_v<decltype(g)>);
    }
    std::println("bank: total {}, entries {}, audits {}", bank.total, bank.entries, bank.audits);

    Counter counter;
    for (int i = 0; i < 3; ++i) lock::locked(counter)->hits += 2;
    std::println("counter hits {}", counter.hits);

    std::println("check Account: {}", lock::check<Account>());
    std::println("check Bank: {}", lock::check<Bank>());
    std::println("check Plain: {}", lock::check<Plain>());
    std::println("check Foreign: {}", lock::check<Foreign>());
    std::println("check NotLockable: {}", lock::check<NotLockable>());
    std::println("check Twice: {}", lock::check<Twice>());
    std::println("can lock: Bank {}, Plain {}, Foreign {}, NotLockable {}, Twice {}", can_lock<Bank>, can_lock<Plain>,
                 can_lock<Foreign>, can_lock<NotLockable>, can_lock<Twice>);
    std::println("can lock Bank by spare_mutex {}, by name {}", can_lock_spare<Bank>, can_lock_name<Bank>);
}
```

It must print exactly:

```
open
  lock account
  unlock account
deposit
  lock account
  inside: balance 125, held true
  unlock account
after: held false
report
  lock account
  open 100
  deposit 25
  unlock account
whole bank
  lock ledger
  lock audit
  sees total true, audits true, name false
  held: ledger true, audit true, spare false
  unlock audit
  unlock ledger
audit only
  lock audit
  sees total false, audits true
  held: ledger false, audit true
  unlock audit
ledger only
  lock ledger
  total 500, entries 1
  sees total true, audits false
  copyable false
  unlock ledger
bank: total 500, entries 1, audits 2
counter hits 6
check Account: ok
check Bank: ok
check Plain: ok
check Foreign: Foreign::y: guard is not a data member of Foreign
check NotLockable: NotLockable::bad: guard cannot be locked
check Twice: Twice::z: guarded more than once
can lock: Bank true, Plain true, Foreign false, NotLockable false, Twice false
can lock Bank by spare_mutex false, by name false
```

Write the complete header to `solution.hpp` in the current directory.
