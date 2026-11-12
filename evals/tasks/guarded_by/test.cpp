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
