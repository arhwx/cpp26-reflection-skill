#include "solution.hpp"

#include <print>
#include <stdexcept>
#include <string>
#include <string_view>

namespace audit {
struct note {
    int level;
};
}  // namespace audit

// user decorators: any annotation type with a member type named decorator
struct scale {
    using decorator = void;
    int factor;
    auto wrap(const deco::frame&, auto next, auto&... args) const { return next(args...) * factor; }
};

struct offset {
    using decorator = void;
    int by;
    auto wrap(const deco::frame&, auto next, auto&... args) const { return next(args...) + by; }
};

struct show_frame {
    using decorator = void;
    auto wrap(const deco::frame& f, auto next, auto&... args) const {
        std::print("frame {} [", f.name);
        for (std::string_view p : f.params) std::print(" {}", p);
        std::println(" ]");
        return next(args...);
    }
};

struct twice {  // calls the rest of the chain two times
    using decorator = void;
    void wrap(const deco::frame&, auto next, auto&... args) const {
        next(args...);
        next(args...);
    }
};

constexpr deco::retry three{3};
int body_calls = 0;
int failures = 0;

namespace impl {
[[=deco::memo{}]] [[=deco::trace{}]] long fib(long n) {
    ++body_calls;
    return n < 2 ? n : fib(n - 1) + fib(n - 2);
}
[[=deco::trace{}]] [[=deco::memo{}]] long square(long x) {
    ++body_calls;
    return x * x;
}
[[=deco::memo{}]] int doubled(int x) { return 2 * x; }
[[=deco::memo{}]] int tripled(int x) { return 3 * x; }

[[=three]] [[=audit::note{1}]] [[=deco::trace{}]] int fetch(int id) {
    if (failures-- > 0) throw std::runtime_error("timeout");
    return id * 10;
}

[[=scale{10}]] [[=audit::note{2}]] [[=offset{1}]] [[=show_frame{}]] int add(int a, int b) { return a + b; }
[[=offset{1}]] [[=scale{10}]] int add2(int a, int b) { return a + b; }
[[=audit::note{3}]] int plain(int x) { return x + 1; }

[[=deco::trace{}]] [[=twice{}]] void greet(const std::string& who) { std::println("hello {}", who); }
}  // namespace impl

struct Bank {
    [[=deco::trace{}]] static double fee(double amount, int percent) { return amount * percent / 100; }
};

constexpr deco::fn<^^impl::fib> fib;
constexpr deco::fn<^^impl::square> square;
constexpr deco::fn<^^impl::doubled> doubled;
constexpr deco::fn<^^impl::tripled> tripled;
constexpr deco::fn<^^impl::fetch> fetch;
constexpr deco::fn<^^impl::add> add;
constexpr deco::fn<^^impl::add2> add2;
constexpr deco::fn<^^impl::plain> plain;
constexpr deco::fn<^^impl::greet> greet;
constexpr deco::fn<^^Bank::fee> fee;

namespace impl {
// calls the decorated objects, not the plain functions
[[=deco::trace{}]] long total(long a, long b) {
    long sum = ::fib(a);
    sum += ::square(b);
    sum += static_cast<long>(::fee(200, static_cast<int>(a)));
    return sum;
}
}  // namespace impl
constexpr deco::fn<^^impl::total> total;

template <class F, class... A>
concept callable = requires(const F& f, A... a) { f(a...); };

int main() {
    std::println("== memo outside trace");
    std::println("fib(20) = {}", fib(20));
    std::println("fib(20) = {}", fib(20));
    std::println("body calls: {}", body_calls);

    std::println("== trace outside memo");
    body_calls = 0;
    std::println("square(7) = {}", square(7));
    std::println("square(7) = {}", square(7));
    std::println("body calls: {}", body_calls);
    std::println("doubled(4) = {}", doubled(4));
    std::println("tripled(4) = {}", tripled(4));

    std::println("== retry");
    failures = 2;
    std::println("fetch(4) = {}", fetch(4));
    failures = 5;
    try {
        fetch(5);
    } catch (const std::exception& e) {
        std::println("gave up: {}", e.what());
    }

    std::println("== user decorators");
    std::println("add(2, 3) = {}", add(2, 3));
    std::println("add2(2, 3) = {}", add2(2, 3));
    std::println("plain(1) = {}", plain(1));
    greet("ada");

    std::println("== nesting");
    std::println("total(10, 3) = {}", total(10, 3));

    std::println("== signatures");
    std::println("fib(int): {}", callable<decltype(fib), int>);
    std::println("fib(const char*): {}", callable<decltype(fib), const char*>);
    std::println("fib(int, int): {}", callable<decltype(fib), int, int>);
    std::println("greet(const char*): {}", callable<decltype(greet), const char*>);
    std::println("fee(int, int): {}", callable<decltype(fee), int, int>);
    std::println("fee(): {}", callable<decltype(fee)>);
}
