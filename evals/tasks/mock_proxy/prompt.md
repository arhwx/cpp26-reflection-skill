I want test doubles for interfaces without writing them by hand and without macros. An interface is a plain struct of member function declarations. From it I want two things: a mock whose methods record their calls and answer as configured, and a logging proxy that forwards each call to a real object and prints the call. Code under test is a template, so it works with the real object, the mock and the proxy alike, using normal call syntax like `store.get(key)`.

Everything goes in `namespace mocks`.

Interfaces:
- The methods of an interface are its named non-static member functions, in declaration order. Constructors, the destructor, assignment operators and other operators are not methods. The methods are only declared and are never called.
- Within one interface, method names are distinct. Return types are `void` or non-reference types that are default constructible and copyable, and parameter types are copyable.

`template <class I> mocks::mock`: a default-constructible type with one public data member per method of `I`, named after the method. For a method `f` with return type `R` and a mock `m`:
- `m.f(args...)` takes exactly the parameter types of `f`, so a call whose arguments do not convert to them does not compile (the test checks this with concepts), and returns `R`. It works through a `const` mock too. Each call first records a copy of its arguments, then calls the behavior set for `f` with the arguments and returns its result. When no behavior was set, it returns a value-initialized `R` (`R{}`), or nothing when `R` is `void`.
- `m.f.returns(value)`: from now on, `f` returns a copy of `value` converted to `R`. Only used for methods that do not return `void`.
- `m.f.invokes(fn)`: from now on, `f` calls `fn` with the arguments and returns its result converted to `R`. `fn` is a callable that the mock keeps a copy of.
- A later `returns` or `invokes` replaces the earlier behavior.
- `m.f.count()`: the number of calls to `f`, as `std::size_t`.
- `m.f.calls()`: a `const` reference to a `std::vector` with one `std::tuple` per call, in call order, holding the recorded arguments. The tuple's element types are the parameter types of `f` with references and `const` removed.

`template <class I, class T> auto mocks::logged(T& object)`: returns a logging proxy that refers to `object`, which outlives it. The proxy has one public data member per method of `I`, named after the method. For a method `f` and a proxy `p`:
- `p.f(args...)` takes exactly the parameter types of `f`, calls the member function of `object` that has the same name and the same parameter types as `f`, and returns its result. `T` may have more member functions, including other overloads with the same name, like `MemoryStore::put(std::string_view, double)`. When `T` has no such member function for some method of `I`, `logged` must not compile.
- After the call returns, the proxy prints one line to standard output: `[log] <method>(<arguments>)` when `f` returns `void`, and `[log] <method>(<arguments>) -> <result>` otherwise.
- `<arguments>` is one `<name>=<value>` per parameter, separated by `, `. `<name>` is the parameter's name in the interface's declaration, not in `T`, or `arg<i>` with the zero-based index `i` when the interface leaves the parameter unnamed.
- A value of type `std::string`, `std::string_view` or `const char*` is printed in double quotes. Any other value is printed as `std::format("{}", value)` would print it.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
bump: 42
get calls: 1, put calls: 1
put got hits = 42
unset size: 0, unset describe: ''
erase: true false after 2 calls, last key keep
get through const: 5
get calls: 2
mock clock reached 1600 after 2 sleeps
--- logged
[log] get(key="hits") -> 0
[log] put(key="hits", value=1)
[log] get(key="hits") -> 1
[log] put(key="hits", value=2)
[log] put(key="ratio", value=3)
[log] erase(key="ratio") -> true
[log] erase(key="missing") -> false
[log] size() -> 1
[log] describe() -> "1 keys"
real store has 1 keys, hits = 2
[log] now() -> 1000
  (sleeping 250 ms)
[log] sleep(arg0=250, reason="poll")
[log] now() -> 1250
  (sleeping 250 ms)
[log] sleep(arg0=250, reason="poll")
[log] now() -> 1500
[log] now() -> 1500
reached 1500
--- constraints
put takes text: false
put takes int: true
has rename: false
logged put takes int: true
```

Write the complete header to `solution.hpp` in the current directory.
