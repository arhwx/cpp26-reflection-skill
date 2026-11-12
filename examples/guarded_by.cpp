// guarded_by: each member names the mutex that guards it. lock::locked(obj) takes those locks and returns a view
// that reaches only the guarded members, so guarded data is out of reach unless its lock is held
#include <algorithm>
#include <meta>
#include <mutex>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace lock {
using std::meta::info;

struct guarded_by {  // the annotation: [[=lock::guarded_by(^^some_mutex)]]
    info mutex;
};

constexpr auto all = std::meta::access_context::unchecked();  // guarded members are usually private
consteval std::vector<info> members(info type) { return nonstatic_data_members_of(type, all); }

consteval info guard_of(info m) {  // the mutex named on member m, or a null reflection
    for (info a : annotations_of(m))
        if (remove_const(type_of(a)) == ^^guarded_by) return extract<guarded_by>(a).mutex;
    return {};
}

template <class M>
constexpr bool lockable = requires(M& m) { m.lock(); m.unlock(); };

consteval std::string_view check(info type) {  // "ok", or what is wrong with the first bad annotation
    std::string name(identifier_of(type));
    for (info m : members(type)) {
        info g = guard_of(m);
        if (g == info{}) continue;
        std::string where = name + "::" + std::string(identifier_of(m)) + ": ";
        if (!std::ranges::contains(members(type), g))
            return std::define_static_string(where + "guard is not a member of " + name);
        if (!extract<bool>(substitute(^^lockable, {type_of(g)})))
            return std::define_static_string(where + "guard cannot be locked");
    }
    return "ok";
}

consteval std::vector<info> exposed(info type, info mutex) {  // members guarded by mutex, or by any mutex if null
    std::vector<info> out;
    for (info m : members(type))
        if (info g = guard_of(m); g != info{} && (mutex == info{} || g == mutex)) out.push_back(m);
    return out;
}
consteval std::vector<info> to_lock(info type, info mutex) {  // the mutexes they need, each once, in declaration order
    std::vector<info> needed, out;
    for (info m : exposed(type, mutex)) needed.push_back(guard_of(m));
    for (info m : members(type))
        if (std::ranges::contains(needed, m)) out.push_back(m);
    return out;
}

// struct type { long& total; std::vector<std::string>& log; ... }, one reference per exposed member
template <class T, info Mutex>
struct fields {
    struct type;
    consteval {
        std::vector<info> specs;
        for (info m : exposed(^^T, Mutex))
            specs.push_back(data_member_spec(add_lvalue_reference(type_of(m)), {.name = identifier_of(m)}));
        define_aggregate(^^type, specs);
    }
};

template <class T, info Mutex>
class view {  // holds the locks while it lives; g->member reaches the exposed members
    static constexpr auto guarded = std::define_static_array(exposed(^^T, Mutex));
    static constexpr auto locks = std::define_static_array(to_lock(^^T, Mutex));
    static constexpr auto unlocks = std::define_static_array(to_lock(^^T, Mutex) | std::views::reverse);
    using refs = fields<T, Mutex>::type;
    T& obj;
    refs r;

    template <std::size_t... I>
    static refs bind(T& o, std::index_sequence<I...>) { return {o.[:guarded[I]:]...}; }

public:
    explicit view(T& o) : obj(o), r(bind(o, std::make_index_sequence<guarded.size()>())) {
        template for (constexpr info m : locks) obj.[:m:].lock();
    }
    ~view() {
        template for (constexpr info m : unlocks) obj.[:m:].unlock();
    }
    view(const view&) = delete;
    refs* operator->() { return &r; }
};

template <class T>  // every guarded member
    requires(check(^^T) == "ok")
view<T, info{}> locked(T& obj) { return view<T, info{}>(obj); }

template <info Mutex, class T>  // only the members guarded by Mutex
    requires(check(^^T) == "ok" && !exposed(^^T, Mutex).empty())
view<T, Mutex> locked(T& obj) { return view<T, Mutex>(obj); }
}  // namespace lock

// ---- user code. rust: struct Bank { ledger: Mutex<Ledger>, audits: Mutex<i32> }
struct noisy_mutex {  // prints what happens to it
    const char* name;
    void lock() { std::println("  lock {}", name); }
    void unlock() { std::println("  unlock {}", name); }
};

class Bank {
public:
    noisy_mutex ledger{"ledger"}, audit{"audit"};

    void deposit(long amount) {
        auto g = lock::locked<^^ledger>(*this);  // takes only the ledger lock
        g->total += amount;
        g->log.push_back("deposit " + std::to_string(amount));
    }

private:
    [[=lock::guarded_by(^^ledger)]] long total = 0;
    [[=lock::guarded_by(^^ledger)]] std::vector<std::string> log;
    [[=lock::guarded_by(^^audit)]] int audits = 0;
};

class Counter {  // a real mutex, shared by threads
    std::mutex m;
    [[=lock::guarded_by(^^m)]] int hits = 0;
};

struct Broken {
    int flag = 0;
    [[=lock::guarded_by(^^flag)]] int data = 0;
};
struct Stray {
    [[=lock::guarded_by(^^Bank::ledger)]] int data = 0;
};

// does the expression compile? each assert below says no
template <class T> concept total_in_reach = requires(T& t) { t.total; };
template <class G> concept view_has_total = requires(G& g) { g->total; };
template <class T> concept can_lock = requires(T& t) { lock::locked(t); };
static_assert(!total_in_reach<Bank>);                  // bank.total is private: the lock is the only way in
static_assert(!can_lock<Broken> && !can_lock<Stray>);  // lock::locked(broken) does not compile, check() says why

int main() {
    Bank bank;
    std::println("deposit");
    bank.deposit(100);

    std::println("audit");
    {
        auto g = lock::locked(bank);  // every mutex that guards something, in declaration order
        g->audits += 1;
        for (const std::string& line : g->log) std::println("  {}", line);
        std::println("  total {}, audits {}", g->total, g->audits);
    }  // unlocked in reverse order

    std::println("audits only");
    {
        auto g = lock::locked<^^Bank::audit>(bank);
        std::println("  audits {}", g->audits);
        static_assert(!view_has_total<decltype(g)>);  // total needs the ledger lock, which this view does not hold
    }

    Counter counter;
    {
        std::vector<std::jthread> threads;
        for (int t = 0; t < 4; ++t)
            threads.emplace_back([&] { for (int i = 0; i < 10000; ++i) lock::locked(counter)->hits += 1; });
    }
    std::println("hits {}", lock::locked(counter)->hits);

    std::println("{}", lock::check(^^Broken));
    std::println("{}", lock::check(^^Stray));
}

// output:
// deposit
//   lock ledger
//   unlock ledger
// audit
//   lock ledger
//   lock audit
//   deposit 100
//   total 100, audits 1
//   unlock audit
//   unlock ledger
// audits only
//   lock audit
//   audits 1
//   unlock audit
// hits 40000
// Broken::data: guard cannot be locked
// Stray::data: guard is not a member of Stray
