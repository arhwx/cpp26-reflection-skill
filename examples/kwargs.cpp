// kwargs: python keyword arguments for ordinary functions. the argument struct is generated from the function's
// own parameter names, so a missing required argument or a misspelled name does not compile
#include <algorithm>
#include <cstddef>
#include <format>
#include <meta>
#include <optional>
#include <print>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace kw {
using std::meta::info;

template <class T>
struct required {  // no default constructor, so the argument must be given
    T value;
    template <class U>
        requires std::is_convertible_v<U, T>
    constexpr required(U&& u) : value(std::forward<U>(u)) {}
};
template <class T>
struct maybe : std::optional<T> {  // empty when the caller leaves the argument out
    using std::optional<T>::optional;
};
template <class T> T& unwrap(T& ref) { return ref; }
template <class T> T& unwrap(required<T>& r) { return r.value; }
template <class T> T& unwrap(maybe<T>& m) { return *m; }
template <class T> bool given(const T&) { return true; }
template <class T> bool given(const maybe<T>& m) { return m.has_value(); }

// a T& parameter stays a reference to the caller's object, a parameter with a default argument may be left out
consteval info member_type(info param) {
    info t = type_of(param);
    if (is_lvalue_reference_type(t) && !is_const_type(remove_reference(t))) return t;
    return substitute(has_default_argument(param) ? ^^maybe : ^^required, {remove_cvref(t)});
}

template <info F>
struct holder {
    static constexpr std::string_view name = identifier_of(F);
    static constexpr auto params = std::define_static_array(parameters_of(F));
    static constexpr std::size_t first_default = std::ranges::count_if(params, [](info p) { return !has_default_argument(p); });
    struct type;
    consteval {  // struct type { <member_type> <parameter name>; ... };
        std::vector<info> specs;
        for (info p : params) specs.push_back(data_member_spec(member_type(p), {.name = identifier_of(p)}));
        define_aggregate(^^type, specs);
    }
};
template <info F>
using args = holder<F>::type;

template <info F, std::size_t... I>  // F's own default arguments fill in the rest
decltype(auto) call_first(args<F>& a, std::index_sequence<I...>) {
    auto& [... values] = a;
    return [:F:](unwrap(values...[I])...);
}

template <info F>
typename [:return_type_of(F):] call(args<F> a) {
    using h = holder<F>;
    auto& [... values] = a;
    std::size_t count = sizeof...(values);  // the arguments before the first one left out
    std::string_view gap;
    template for (constexpr std::size_t i : std::define_static_array(std::views::iota(h::first_default, h::params.size()))) {
        constexpr std::string_view param = identifier_of(h::params[i]);
        if (!given(values...[i]) && gap.empty()) {
            count = i;
            gap = param;
        } else if (given(values...[i]) && !gap.empty()) {  // reflection cannot read the value of a default argument
            throw std::invalid_argument(std::format("{}: {} must be given because {} is given", h::name, gap, param));
        }
    }
    template for (constexpr std::size_t n : std::define_static_array(std::views::iota(h::first_default, h::params.size() + 1))) {
        if (n == count) return call_first<F>(a, std::make_index_sequence<n>{});
    }
    std::unreachable();
}
}  // namespace kw

// ---- user code. python: def greet(name, times=1, shout=False), called as greet(name="ann", times=3)
double area(double width, double height) { return width * height; }

std::string greet(const std::string& name, int times = 1, bool shout = false) {
    std::string text = name;
    for (int i = 1; i < times; ++i) text += " " + name;
    return shout ? text + "!" : text;
}

void fill(std::vector<int>& out, int count, int start = 0, int step = 1) {  // appends to the caller's vector
    for (int i = 0; i < count; ++i) out.push_back(start + i * step);
}

// kw::args<^^greet> is struct { kw::required<std::string> name; kw::maybe<int> times; kw::maybe<bool> shout; }
// python raises a TypeError at run time for these calls. here they do not compile
template <auto F> constexpr bool no_height = requires { kw::call<F>({.width = 1.0}); };
template <auto F> constexpr bool typo = requires { kw::call<F>({.width = 1.0, .hieght = 2.0}); };
template <auto F> constexpr bool wrong_type = requires { kw::call<F>({.width = "wide", .height = 2.0}); };
template <auto F> constexpr bool temporary = requires { kw::call<F>({.out = std::vector<int>{}, .count = 1}); };
static_assert(!no_height<^^area>);   // height is required
static_assert(!typo<^^area>);        // there is no hieght
static_assert(!wrong_type<^^area>);  // "wide" is not a double
static_assert(!temporary<^^fill>);   // out must be a vector the caller owns

int main() {
    std::println("area: {}", kw::call<^^area>({.width = 3, .height = 4.5}));
    std::println("greet: {}", kw::call<^^greet>({.name = "ann"}));
    std::println("greet: {}", kw::call<^^greet>({.name = "ann", .times = 3}));
    std::println("greet: {}", kw::call<^^greet>({.name = "ann", .times = 2, .shout = true}));

    std::vector<int> v;
    kw::call<^^fill>({.out = v, .count = 3, .start = 5, .step = 2});
    kw::call<^^fill>({.out = v, .count = 2});  // start and step use the defaults in fill
    std::println("fill: {}", v);

    kw::args<^^area> saved{.width = 2, .height = 5};  // python: saved = dict(width=2, height=5); area(**saved)
    std::println("saved: {}", kw::call<^^area>(saved));

    try {  // python passes the default for times here. c++ can only leave out trailing arguments
        kw::call<^^greet>({.name = "ann", .shout = true});
    } catch (const std::invalid_argument& e) {
        std::println("error: {}", e.what());
    }
}

// output:
// area: 13.5
// greet: ann
// greet: ann ann ann
// greet: ann ann!
// fill: [5, 7, 9, 0, 1]
// saved: 10
// error: greet: times must be given because shout is given
