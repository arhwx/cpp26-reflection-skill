#include "solution.hpp"

#include <print>
#include <string>
#include <type_traits>

class Counter {
public:
    Counter() { std::println("  Counter created"); }
    ~Counter() { std::println("  Counter destroyed with total {}", total_); }

    void add(int amount) {
        total_ += amount;
        ++count_;
    }
    int total() const { return total_; }
    double mean(int) const { return count_ == 0 ? 0.0 : double(total_) / count_; }
    bool reset(bool keep_count, long start) {
        total_ = int(start);
        if (!keep_count) count_ = 0;
        return count_ != 0;
    }
    void copy_to(int* out, unsigned times) const {
        for (unsigned i = 0; i < times; ++i) out[i] = total_;
    }
    const char* label() const { return total_ < 10 ? "small" : "large"; }

    static int version() { return 2; }
    Counter& operator+=(int amount) {
        add(amount);
        return *this;
    }

private:
    void bump() { ++total_; }
    int total_ = 0;
    int count_ = 0;
};

struct Silent {};

struct Overloaded {
    void set(int) {}
    void set(double) {}
};

struct Stringy {
    int size() const { return 0; }
    void rename(const char* from, std::string to) {}
};

struct ByRef {
    int& slot() { return value; }
    int value = 0;
};

struct Configured {
    explicit Configured(int level) : level(level) {}
    int get() const { return level; }
    int level;
};

using CounterTable = plugin::vtable<Counter>;
static_assert(std::is_same_v<decltype(CounterTable::name), const char*>);
static_assert(std::is_same_v<decltype(CounterTable::create), void* (*)()>);
static_assert(std::is_same_v<decltype(CounterTable::destroy), void (*)(void*)>);
static_assert(std::is_same_v<decltype(CounterTable::add), void (*)(void*, int)>);
static_assert(std::is_same_v<decltype(CounterTable::total), int (*)(const void*)>);
static_assert(std::is_same_v<decltype(CounterTable::copy_to), void (*)(const void*, int*, unsigned)>);
static_assert(std::is_same_v<decltype(CounterTable::label), const char* (*)(const void*)>);
static_assert(std::is_standard_layout_v<CounterTable> && std::is_trivially_copyable_v<CounterTable>);
static_assert(sizeof(CounterTable) == 9 * sizeof(void*));
static_assert(sizeof(plugin::vtable<Silent>) == 3 * sizeof(void*));

template <class T>
concept has_table = requires { typename plugin::vtable<T>; };

// a C host sees only the table and a void pointer
void run_host(const CounterTable& t) {
    std::println("host loads plugin {}", t.name);
    void* obj = t.create();
    t.add(obj, 4);
    t.add(obj, 7);
    std::println("total {} mean {} label {}", t.total(obj), t.mean(obj, 0), t.label(obj));
    int out[3] = {};
    t.copy_to(obj, out, 2);
    std::println("copied {} {} {}", out[0], out[1], out[2]);
    bool kept = t.reset(obj, true, 3);
    std::println("reset keeping count: {} total {} mean {}", kept, t.total(obj), t.mean(obj, 0));
    kept = t.reset(obj, false, 5);
    std::println("reset dropping count: {} mean {}", kept, t.mean(obj, 0));
    t.destroy(obj);
}

int main() {
    const CounterTable& t = plugin::table<Counter>();
    std::println("same table: {}", &t == &plugin::table<Counter>());
    run_host(t);

    // two objects behind one table are independent
    void* a = t.create();
    void* b = t.create();
    t.add(a, 1);
    t.add(b, 20);
    std::println("a {} b {}", t.label(a), t.label(b));
    t.destroy(b);
    t.destroy(a);

    std::println("{}", plugin::c_header<Counter>());
    std::println("{}", plugin::c_header<Silent>());
    void* s = plugin::table<Silent>().create();
    plugin::table<Silent>().destroy(s);
    std::println("{} has no methods", plugin::table<Silent>().name);

    std::println("Counter: '{}' {}", plugin::why_not<Counter>(), plugin::exportable<Counter>);
    std::println("Overloaded: '{}' {}", plugin::why_not<Overloaded>(), plugin::exportable<Overloaded>);
    std::println("Stringy: '{}' {}", plugin::why_not<Stringy>(), plugin::exportable<Stringy>);
    std::println("ByRef: '{}' {}", plugin::why_not<ByRef>(), plugin::exportable<ByRef>);
    std::println("Configured: '{}' {}", plugin::why_not<Configured>(), plugin::exportable<Configured>);
    std::println("tables: {} {} {}", has_table<Counter>, has_table<Silent>, has_table<Stringy>);
}
