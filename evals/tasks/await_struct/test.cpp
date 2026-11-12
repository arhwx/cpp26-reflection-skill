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
