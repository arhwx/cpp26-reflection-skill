I miss Python decorators. I want to stack them on plain C++ functions as annotations, `[[=deco::trace{}]] long fib(long n)`, and get a callable object that runs the function through its decorators. A decorator is any annotation type that opts in, so users can write their own next to the three that come with the library.

Everything goes in `namespace deco`.
- `deco::frame`: a struct with two public members: `std::string_view name`, the function's name as written in its declaration (no namespace or class prefix), and `std::span<const std::string_view> params`, the names of its parameters in order.
- `template <std::meta::info F> deco::fn`: a class type for `F`, a reflection of a namespace-scope function or a static member function. It is default constructible in constant expressions (`constexpr deco::fn<^^impl::fib> fib;`). Its call operator is `const`, takes exactly the parameter types of `F` and returns `F`'s return type: arguments are converted as they would be for a direct call of `F`, and a call that `F` would not accept is not a valid expression, so a requires-expression that tries it is `false` (the test checks this with a concept).
- Calling it runs the decorator chain of `F` and returns its result.

Decorators:
- A decorator is an annotation on `F` whose type, without `const`, is a class type with a member type named `decorator`. All other annotations are ignored.
- The decorators of `F` form a chain in the order they are written: the first one written is the outermost and runs first, as in Python. A function with no decorators is simply called.
- Each decorator `d` is a `const` object equal to the annotation's value. It is called as `d.wrap(frame, next, args...)`, where `frame` is the `deco::frame` of `F` (passed as a `const deco::frame&`), `args...` are lvalues holding the call's arguments, already converted to `F`'s parameter types (the test's decorators take them as `auto&...`), and `next` is a copyable callable object that runs the rest of the chain, which is the next decorator or finally `F` itself, with the arguments it is given converted to `F`'s parameter types, and returns `F`'s return type. `wrap` may call `next` any number of times.
- What the outermost `wrap` returns, converted to `F`'s return type, is the result of the call. When `F` returns `void`, the result of `wrap` is ignored.

Library decorators, each with a member type `decorator`:
- `deco::trace{}`: before calling `next`, prints the line `<indent>-> <name>(<p1>=<v1>, <p2>=<v2>)`, with `<name>()` when there are no parameters. When `next` returns, prints `<indent><- <name> = <result>`, or `<indent><- <name>` when the return type is `void`, and returns the result. When `next` throws an exception derived from `std::exception`, prints `<indent><- <name> threw <what()>` and rethrows the same exception. Values are written with `std::format("{}", value)`. `<indent>` is two spaces for each `trace` call, over all functions, that has started and not yet finished, not counting this one. The program is single threaded.
- `deco::memo{}`: one cache per decorated function, keyed by the argument values (compared with `<`). When the arguments are in the cache, returns the cached result without calling `next`. Otherwise calls `next`, stores the result and returns it. Two different functions never share a cache, even with the same signature. It is only used on functions that return a value.
- `deco::retry{n}`: an aggregate with one member `int times`, usable as a `constexpr` variable as in the test. It calls `next`; when that throws an exception derived from `std::exception`, it prints `retry <name>: attempt <i> of <times> failed: <what()>`, where `<i>` counts attempts from 1, and calls `next` again. After the failure of attempt `times` it rethrows that exception instead of trying again.

All output goes to standard output with `std::println` or equivalent. Annotation spellings must compile exactly as they appear in the test.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
== memo outside trace
-> fib(n=20)
<- fib = 6765
fib(20) = 6765
fib(20) = 6765
body calls: 21891
== trace outside memo
-> square(x=7)
<- square = 49
square(7) = 49
-> square(x=7)
<- square = 49
square(7) = 49
body calls: 1
doubled(4) = 8
tripled(4) = 12
== retry
-> fetch(id=4)
<- fetch threw timeout
retry fetch: attempt 1 of 3 failed: timeout
-> fetch(id=4)
<- fetch threw timeout
retry fetch: attempt 2 of 3 failed: timeout
-> fetch(id=4)
<- fetch = 40
fetch(4) = 40
-> fetch(id=5)
<- fetch threw timeout
retry fetch: attempt 1 of 3 failed: timeout
-> fetch(id=5)
<- fetch threw timeout
retry fetch: attempt 2 of 3 failed: timeout
-> fetch(id=5)
<- fetch threw timeout
retry fetch: attempt 3 of 3 failed: timeout
gave up: timeout
== user decorators
frame add [ a b ]
add(2, 3) = 60
add2(2, 3) = 51
plain(1) = 2
-> greet(who=ada)
hello ada
hello ada
<- greet
== nesting
-> total(a=10, b=3)
  -> fib(n=10)
  <- fib = 55
  -> square(x=3)
  <- square = 9
  -> fee(amount=200, percent=10)
  <- fee = 20
<- total = 84
total(10, 3) = 84
== signatures
fib(int): true
fib(const char*): false
fib(int, int): false
greet(const char*): true
fee(int, int): true
fee(): false
```

Write the complete header to `solution.hpp` in the current directory.
