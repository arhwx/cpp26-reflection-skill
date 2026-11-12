#include "solution.hpp"

#include <cstddef>
#include <format>
#include <map>
#include <print>
#include <string>
#include <string_view>
#include <tuple>

// interfaces are plain structs of declarations
struct Store {
    int get(std::string_view key) const;
    void put(std::string_view key, int value);
    bool erase(const std::string& key);
    std::size_t size() const;
    std::string describe() const;
};

struct Clock {
    long now() const;
    void sleep(int, std::string_view reason);
};

// real implementations know nothing about the interfaces
class MemoryStore {
public:
    int get(std::string_view k) const {
        auto it = data_.find(std::string(k));
        return it == data_.end() ? 0 : it->second;
    }
    void put(std::string_view k, int v) { data_[std::string(k)] = v; }
    void put(std::string_view k, double v) { data_[std::string(k)] = static_cast<int>(v * 100); }
    bool erase(const std::string& k) { return data_.erase(k) > 0; }
    std::size_t size() const { return data_.size(); }
    std::string describe() const { return std::format("{} keys", data_.size()); }

private:
    std::map<std::string, int> data_;
};

struct FakeClock {
    long t = 1000;
    long now() const { return t; }
    void sleep(int ms, std::string_view) {
        std::println("  (sleeping {} ms)", ms);
        t += ms;
    }
};

// code under test works with anything that has the methods
template <class S>
int bump(S& store, std::string_view key) {
    int next = store.get(key) + 1;
    store.put(key, next);
    return next;
}

template <class C>
long wait_until(C& clock, long deadline) {
    while (clock.now() < deadline) clock.sleep(250, "poll");
    return clock.now();
}

template <class M>
concept put_takes_text = requires(M& m) { m.put("k", "v"); };
template <class M>
concept put_takes_int = requires(M& m) { m.put("k", 7); };
template <class M>
concept has_rename = requires(M& m) { m.rename("a", "b"); };

int main() {
    mocks::mock<Store> store;
    store.get.returns(41);
    std::println("bump: {}", bump(store, "hits"));
    std::println("get calls: {}, put calls: {}", store.get.count(), store.put.count());
    auto [key, value] = store.put.calls()[0];
    std::println("put got {} = {}", key, value);
    std::println("unset size: {}, unset describe: '{}'", store.size(), store.describe());

    store.erase.invokes([](const std::string& k) { return k.starts_with("tmp"); });
    bool a = store.erase("tmp1");
    bool b = store.erase("keep");
    std::println("erase: {} {} after {} calls, last key {}", a, b, store.erase.count(), std::get<0>(store.erase.calls()[1]));

    store.get.invokes([](std::string_view k) { return static_cast<int>(k.size()); });
    const mocks::mock<Store>& view = store;
    std::println("get through const: {}", view.get("seven"));
    std::println("get calls: {}", store.get.count());

    mocks::mock<Clock> clock;
    long ticks = 0;
    clock.now.invokes([&ticks] { return ticks += 400; });
    long reached = wait_until(clock, 1000);
    std::println("mock clock reached {} after {} sleeps", reached, clock.sleep.count());

    std::println("--- logged");
    MemoryStore real;
    auto logged = mocks::logged<Store>(real);
    bump(logged, "hits");
    bump(logged, "hits");
    logged.put("ratio", 3);
    logged.erase("ratio");
    logged.erase("missing");
    logged.size();
    logged.describe();
    std::println("real store has {} keys, hits = {}", real.size(), real.get("hits"));

    FakeClock fake;
    auto timed = mocks::logged<Clock>(fake);
    std::println("reached {}", wait_until(timed, 1400));

    std::println("--- constraints");
    std::println("put takes text: {}", put_takes_text<mocks::mock<Store>>);
    std::println("put takes int: {}", put_takes_int<mocks::mock<Store>>);
    std::println("has rename: {}", has_rename<mocks::mock<Store>>);
    std::println("logged put takes int: {}", put_takes_int<decltype(logged)>);
}
