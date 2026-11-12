// decorators: python style decorators written as annotations on plain functions. a decorator is any
// annotation type with a wrap member; the first one written is the outermost, and they can take arguments
#include <map>
#include <meta>
#include <print>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

// ---- the library
namespace deco {
// a decorator is any annotation type with a member "using decorator = void;"
template <class T>
constexpr bool is_decorator = requires { typename T::decorator; };

template <std::meta::info F>
constexpr std::string_view name = std::define_static_string(std::meta::identifier_of(F));

// decorator types found on F, top to bottom
template <std::meta::info F>
consteval std::vector<std::meta::info> list() {
    std::vector<std::meta::info> out;
    for (auto a : std::meta::annotations_of(F))
        if (std::meta::extract<bool>(std::meta::substitute(
                ^^is_decorator, {std::meta::remove_const(std::meta::type_of(a))})))
            out.push_back(a);
    return out;
}

// apply decorator I around the rest of the chain; the first one written is the outermost, like python
template <std::meta::info F, std::size_t I = 0, class... A>
auto run(A... args) {
    static constexpr auto ds = std::define_static_array(list<F>());
    if constexpr (I == ds.size()) {
        return [:F:](args...);
    } else {
        using D = [:std::meta::remove_const(std::meta::type_of(ds[I])):];
        constexpr D d = std::meta::extract<D>(ds[I]);
        return d.template wrap<F>([](auto... a) { return run<F, I + 1>(a...); }, args...);
    }
}

template <std::meta::info F>
struct fn {
    template <class... A>
    auto operator()(A... a) const { return run<F>(a...); }
};
}  // namespace deco

// ---- decorators: anyone can write one
struct log {
    using decorator = void;
    template <std::meta::info F>
    auto wrap(auto next, auto... a) const {
        auto r = next(a...);
        std::println("  log: {}{} = {}", deco::name<F>, std::tuple{a...}, r);
        return r;
    }
};

struct cache {
    using decorator = void;
    template <std::meta::info F>
    auto wrap(auto next, auto... a) const {
        static std::map<std::tuple<decltype(a)...>, decltype(next(a...))> memo;
        if (auto it = memo.find({a...}); it != memo.end()) return it->second;
        return memo[{a...}] = next(a...);
    }
};

struct retry {
    using decorator = void;
    int times;
    template <std::meta::info F>
    auto wrap(auto next, auto... a) const {
        for (int i = 1;; ++i) {
            try { return next(a...); }
            catch (const std::exception& e) {
                std::println("  retry: {} failed ({}), attempt {}/{}", deco::name<F>, e.what(), i, times);
                if (i == times) throw;
            }
        }
    }
};

struct note { const char* text; };   // an unrelated annotation, ignored

// ---- user code
namespace impl {
[[=log{}]] [[=cache{}]] long fib(long n) { return n < 2 ? n : fib(n - 1) + fib(n - 2); }

int failures = 2;
[[=retry{3}]] [[=note{"flaky"}]] int fetch(int id) {
    if (failures-- > 0) throw std::runtime_error("timeout");
    return id * 10;
}
}
constexpr deco::fn<^^impl::fib> fib;
constexpr deco::fn<^^impl::fetch> fetch;

int main() {
    std::println("fib(30) = {}", fib(30));
    std::println("fib(30) = {}", fib(30));
    std::println("fetch(4) = {}", fetch(4));
}

// output:
//   log: fib(30) = 832040
// fib(30) = 832040
//   log: fib(30) = 832040
// fib(30) = 832040
//   retry: fetch failed (timeout), attempt 1/3
//   retry: fetch failed (timeout), attempt 2/3
// fetch(4) = 40
