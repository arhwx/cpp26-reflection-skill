I want `Promise.all` for a struct, like JavaScript's `Promise.all` on an object or Swift's `async let`: I put several awaitables in a struct, await the struct once, and get back a struct of results with the same member names. The result type must be generated from the request struct, so nobody writes it by hand.

The header must not depend on any particular task or event loop type. The test brings its own (`demo::task`, `demo::loop` and `demo::ready`), defined after it includes the header. Your header only provides what is listed below.

Everything goes in `namespace aio`:
- An awaitable, for this header, is a class type (cv-qualifiers ignored) that has public members named `await_ready`, `await_suspend` and `await_resume`. Anything else, such as `int` or `std::string`, is not awaitable.
- `template <class Tasks> consteval std::string_view aio::check()` (`constexpr` is fine too): returns `ok` if every non-static data member of `Tasks` is awaitable. Otherwise it returns `<Struct>::<member> is not awaitable` for the first member, in declaration order, that is not, where `<Struct>` is the plain name of `Tasks` and `<member>` the member's name.
- `aio::results<Tasks>`: an aggregate struct type with one public non-static data member for each non-static data member of `Tasks`, with the same name and in the same order. Its type is the return type of `await_resume()` called on an lvalue of the member's type, with references and cv-qualifiers removed. Naming `results<Tasks>` again for the same `Tasks` gives the same type.
- `template <class Tasks> auto aio::all(Tasks jobs)`: takes the struct by value and returns an awaitable. `co_await aio::all(jobs)` evaluates to a `results<Tasks>` that holds the value of every member.

How `co_await aio::all(jobs)` runs:
- No member is started before the `co_await`. At the `co_await`, `all` starts the members one after another in declaration order.
- Starting a member means that a separate coroutine, owned by your header, does `co_await` on that member (as an lvalue inside the object that `all` returned) and stores the value it gets into the result member with the same name. The next member is started as soon as this helper suspends or finishes, so all members run at the same time.
- When the last member finishes after the start loop, the awaiting coroutine is resumed right there, from inside the helper that finished last.
- If every member has already finished when the start loop ends, the awaiting coroutine does not suspend and continues at once.

Rules:
- `Tasks` is an aggregate whose members are all public. `results<Tasks>` and `all` are only used with a `Tasks` for which `check` returns `ok`; for any other `Tasks` they may fail to compile.
- Every result type is default constructible and move assignable. No member's `await_resume` returns `void`.
- The object returned by `all` is awaited exactly once, as the temporary in the `co_await` expression.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <coroutine>
#include <exception>
#include <format>
#include <map>
#include <meta>
#include <optional>
#include <print>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace demo {
template <class T>
struct task {  // a lazy coroutine that hands its result to whoever awaits it
    struct promise_type {
        std::optional<T> result;
        std::coroutine_handle<> parent = std::noop_coroutine();
        task get_return_object() { return task{std::coroutine_handle<promise_type>::from_promise(*this)}; }
        std::suspend_always initial_suspend() noexcept { return {}; }
        auto final_suspend() noexcept {
            struct back_to_parent {
                bool await_ready() noexcept { return false; }
                std::coroutine_handle<> await_suspend(std::coroutine_handle<promise_type> h) noexcept {
                    return h.promise().parent;
                }
                void await_resume() noexcept {}
            };
            return back_to_parent{};
        }
        void return_value(T v) { result = std::move(v); }
        void unhandled_exception() { std::terminate(); }
    };
    std::coroutine_handle<promise_type> handle;
    explicit task(std::coroutine_handle<promise_type> h) : handle(h) {}
    task(task&& other) noexcept : handle(std::exchange(other.handle, {})) {}
    ~task() {
        if (handle) handle.destroy();
    }
    bool await_ready() { return false; }
    std::coroutine_handle<> await_suspend(std::coroutine_handle<> parent) {
        handle.promise().parent = parent;
        return handle;
    }
    T await_resume() { return std::move(*handle.promise().result); }
};

struct loop {  // an event loop on virtual time, so that the output is deterministic
    int now = 0;
    std::multimap<int, std::coroutine_handle<>> timers;
    auto sleep(int ms) {
        struct awaiter {
            loop& self;
            int ms;
            bool await_ready() { return false; }
            void await_suspend(std::coroutine_handle<> h) { self.timers.emplace(self.now + ms, h); }
            void await_resume() {}
        };
        return awaiter{*this, ms};
    }
    template <class T>
    T run(task<T> main) {
        main.handle.resume();
        while (!timers.empty()) {
            auto [when, sleeper] = *timers.begin();
            timers.erase(timers.begin());
            now = when;
            sleeper.resume();
        }
        return std::move(*main.handle.promise().result);
    }
};

struct ready {  // an awaitable that never suspends
    int value;
    bool await_ready() { return true; }
    void await_suspend(std::coroutine_handle<>) {}
    int await_resume() { return value; }
};
}  // namespace demo

struct User {
    std::string name;
};

demo::task<User> fetch_user(demo::loop& loop) {
    std::println("t={} user starts", loop.now);
    co_await loop.sleep(30);
    std::println("t={} user arrived", loop.now);
    co_return User{"ada"};
}
demo::task<std::vector<std::string>> fetch_orders(demo::loop& loop) {
    std::println("t={} orders starts", loop.now);
    co_await loop.sleep(50);
    std::println("t={} orders arrived", loop.now);
    co_return std::vector<std::string>{"engine", "cards"};
}
demo::task<int> fetch_credit(demo::loop& loop) {
    std::println("t={} credit starts", loop.now);
    co_await loop.sleep(30);
    std::println("t={} credit arrived", loop.now);
    co_return 250;
}
demo::task<std::string> at_once(std::string word) {
    std::println("at once: {}", word);
    co_return word;
}

struct Dashboard {  // what to fetch, by name
    demo::task<User> user;
    demo::task<std::vector<std::string>> orders;
    demo::task<int> credit;
};

struct Quick {  // nothing here ever waits
    demo::task<std::string> first;
    demo::ready lucky;
    demo::task<std::string> second;
};

struct Broken {
    demo::task<int> fine;
    int count;
    std::string label;
};

template <class T>
consteval std::string_view member_names() {
    std::string out;
    for (std::meta::info m : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unprivileged()))
        out += (out.empty() ? "" : " ") + std::string(std::meta::identifier_of(m));
    return std::define_static_string(out);
}

demo::task<std::string> render(demo::loop& loop) {
    auto page = co_await aio::all(
        Dashboard{.user = fetch_user(loop), .orders = fetch_orders(loop), .credit = fetch_credit(loop)});
    std::println("t={} dashboard done", loop.now);
    auto quick = co_await aio::all(Quick{.first = at_once("one"), .lucky = {7}, .second = at_once("two")});
    std::println("quick: {} {} {}", quick.first, quick.lucky, quick.second);
    co_return std::format("{} has {} orders and {} credit, ready at t={}", page.user.name, page.orders.size(),
                          page.credit, loop.now);
}

int main() {
    using R = aio::results<Dashboard>;
    std::println("results<Dashboard>: {}", member_names<R>());
    std::println("results<Quick>: {}", member_names<aio::results<Quick>>());
    std::println("user is User: {}", std::is_same_v<decltype(R::user), User>);
    std::println("orders is vector<string>: {}", std::is_same_v<decltype(R::orders), std::vector<std::string>>);
    std::println("credit is int: {}", std::is_same_v<decltype(R::credit), int>);
    std::println("lucky is int: {}", std::is_same_v<decltype(aio::results<Quick>::lucky), int>);
    std::println("same type: {}", std::is_same_v<R, aio::results<Dashboard>>);
    R sample{.user = {"grace"}, .orders = {"a", "b", "c"}, .credit = 3};
    std::println("sample: {} {} {}", sample.user.name, sample.orders.size(), sample.credit);
    std::println("check Dashboard: {}", aio::check<Dashboard>());
    std::println("check Quick: {}", aio::check<Quick>());
    std::println("check Broken: {}", aio::check<Broken>());

    demo::loop loop;
    std::println("{}", loop.run(render(loop)));
}
```

It must print exactly:

```
results<Dashboard>: user orders credit
results<Quick>: first lucky second
user is User: true
orders is vector<string>: true
credit is int: true
lucky is int: true
same type: true
sample: grace 3 3
check Dashboard: ok
check Quick: ok
check Broken: Broken::count is not awaitable
t=0 user starts
t=0 orders starts
t=0 credit starts
t=30 user arrived
t=30 credit arrived
t=50 orders arrived
t=50 dashboard done
at once: one
at once: two
quick: one 7 two
ada has 2 orders and 250 credit, ready at t=50
```

Write the complete header to `solution.hpp` in the current directory.
