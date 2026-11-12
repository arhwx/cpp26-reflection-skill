// quickcheck: property tests for plain structs. random values come from the member types and their
// annotations, and a failing value is shrunk to a small counterexample
#include <algorithm>
#include <cstdint>
#include <format>
#include <meta>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace fuzz {
using std::meta::info;

struct range { long long lo, hi; };  // annotation: the values of an integer member, or of its elements
struct max_size { std::size_t n; };  // annotation: the longest a string or vector member gets

struct options { range bounds{0, 100}; std::size_t max = 3; };  // the defaults, for members without annotations

class rng {  // splitmix64, so a seed gives the same values everywhere
    std::uint64_t state;
public:
    explicit rng(std::uint64_t seed) : state(seed) {}
    std::uint64_t next() {
        std::uint64_t z = state += 0x9e3779b97f4a7c15;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        return z ^ (z >> 31);
    }
};

consteval std::vector<info> fields(info type) { return nonstatic_data_members_of(type, std::meta::access_context::unprivileged()); }
consteval options options_of(info member) {
    options o;
    for (info a : annotations_of(member)) {
        if (remove_const(type_of(a)) == ^^range) o.bounds = extract<range>(a);
        if (remove_const(type_of(a)) == ^^max_size) o.max = extract<max_size>(a).n;
    }
    return o;
}

// bool, integers, enums, strings, vectors of those and structs of those; anything else has no generator
consteval bool supported(info t) {
    t = dealias(t);
    if (is_integral_type(t) || is_enum_type(t) || t == dealias(^^std::string)) return true;
    if (has_template_arguments(t) && template_of(t) == ^^std::vector) return supported(template_arguments_of(t)[0]);
    if (!is_class_type(t)) return false;
    for (info m : fields(t))
        if (!supported(type_of(m))) return false;
    return true;
}
template <class T>
concept arbitrary_type = supported(^^T);

template <class T> constexpr bool is_vector = false;
template <class E> constexpr bool is_vector<std::vector<E>> = true;
template <class E>  // the enumerators of E as values, in declaration order
constexpr auto values = std::define_static_array(enumerators_of(^^E) | std::views::transform([](info e) { return extract<E>(e); }));

template <arbitrary_type T>
T arbitrary(rng& r, options o = {}) {
    if constexpr (std::is_same_v<T, bool>) {
        return r.next() % 2;
    } else if constexpr (std::is_integral_v<T>) {
        return static_cast<T>(o.bounds.lo + static_cast<long long>(r.next() % (o.bounds.hi - o.bounds.lo + 1)));
    } else if constexpr (std::is_enum_v<T>) {
        return values<T>[r.next() % values<T>.size()];
    } else if constexpr (std::is_same_v<T, std::string>) {
        std::string s(r.next() % (o.max + 1), ' ');
        for (char& c : s) c = static_cast<char>('a' + r.next() % 26);
        return s;
    } else if constexpr (is_vector<T>) {
        T v(r.next() % (o.max + 1));
        for (auto&& e : v) e = arbitrary<typename T::value_type>(r, {o.bounds});
        return v;
    } else {
        T v{};
        template for (constexpr info m : std::define_static_array(fields(^^T)))
            v.[:m:] = arbitrary<typename [:type_of(m):]>(r, options_of(m));
        return v;
    }
}

// smaller values close to v, simplest first
template <class T>
std::vector<T> shrinks(const T& v, options o = {}) {
    std::vector<T> out;
    if constexpr (std::is_same_v<T, bool>) {
        if (v) out.push_back(false);
    } else if constexpr (std::is_integral_v<T>) {
        long long x = v, t = std::clamp(0ll, o.bounds.lo, o.bounds.hi);  // t: the allowed value closest to zero
        if (x != t)
            for (long long c : {t, x - (x - t) / 2, x > t ? x - 1 : x + 1})
                if (c != x) out.push_back(static_cast<T>(c));
    } else if constexpr (std::is_enum_v<T>) {
        for (T e : values<T>) {
            if (e == v) break;
            out.push_back(e);
        }
    } else if constexpr (std::is_same_v<T, std::string>) {
        for (std::size_t i = 0; i < v.size(); ++i) out.push_back(std::string(v).erase(i, 1));
        for (std::size_t i = 0; i < v.size(); ++i)
            if (v[i] != 'a') out.push_back(std::string(v).replace(i, 1, "a"));
    } else if constexpr (is_vector<T>) {
        for (std::size_t i = 0; i < v.size(); ++i) out.push_back(v), out.back().erase(out.back().begin() + i);
        for (std::size_t i = 0; i < v.size(); ++i)
            for (const auto& c : shrinks(v[i], {o.bounds})) out.push_back(v), out.back()[i] = c;
    } else {
        template for (constexpr info m : std::define_static_array(fields(^^T)))
            for (const auto& c : shrinks(v.[:m:], options_of(m))) out.push_back(v), out.back().[:m:] = c;
    }
    return out;
}

template <class T>
std::string show(const T& v) {
    if constexpr (std::is_same_v<T, std::string>) {
        return "\"" + v + "\"";
    } else if constexpr (std::is_enum_v<T>) {
        template for (constexpr info e : std::define_static_array(enumerators_of(^^T)))
            if (v == [:e:]) return std::string(identifier_of(e));
        return "?";
    } else if constexpr (std::is_arithmetic_v<T>) {
        return std::format("{}", v);
    } else if constexpr (is_vector<T>) {
        std::string s;
        for (const auto& e : v) s += (s.empty() ? "" : ", ") + show(e);
        return "[" + s + "]";
    } else {
        std::string s;
        template for (constexpr info m : std::define_static_array(fields(^^T)))
            s += std::format("{}{}: {}", s.empty() ? "" : ", ", identifier_of(m), show(v.[:m:]));
        return std::format("{}{{{}}}", identifier_of(^^T), s);
    }
}

template <arbitrary_type T>
void check(std::string_view name, std::uint64_t seed, int runs, auto property) {
    rng r(seed);
    for (int i = 1; i <= runs; ++i) {
        T v = arbitrary<T>(r);
        if (property(v)) continue;
        std::println("{}: falsified by test {}: {}", name, i, show(v));
        int steps = 0;
        while (true) {
            std::vector<T> smaller = shrinks(v);
            auto it = std::ranges::find_if_not(smaller, property);  // the first smaller value that still fails
            if (it == smaller.end()) break;
            v = *it, ++steps;
        }
        std::println("{}: shrunk in {} steps to {}", name, steps, show(v));
        return;
    }
    std::println("{}: passed {} tests", name, runs);
}
}  // namespace fuzz

// ---- user code. haskell: quickCheck prop_boxes, python hypothesis: @given(st.builds(Order))
enum class Size { small, medium, large };

struct Item {
    [[=fuzz::range{1, 20}]] int qty;
    Size size;
};

struct Order {
    [[=fuzz::range{1000, 9999}]] int id;
    [[=fuzz::max_size{4}]] std::string coupon;
    std::vector<Item> items;
    bool gift;
};

// a box holds 10 units. a small item takes 1 unit, a medium one 2 and a large one 5
int units(const Order& o) {
    int n = 0;
    for (const Item& i : o.items) n += i.qty * (i.size == Size::small ? 1 : i.size == Size::medium ? 2 : 5);
    return n;
}
int boxes(const Order& o) { return units(o) / 10; }  // the bug: rounds down
int boxes_fixed(const Order& o) { return (units(o) + 9) / 10; }

struct Link { int value; Link* next; };  // a pointer member has no generator, so check<Link> does not compile
static_assert(fuzz::arbitrary_type<Order> && !fuzz::arbitrary_type<Link>);

int main() {
    fuzz::check<Order>("boxes hold every unit", 3, 1000, [](const Order& o) { return boxes(o) * 10 >= units(o); });
    fuzz::check<Order>("fixed boxes hold every unit", 3, 1000, [](const Order& o) { return boxes_fixed(o) * 10 >= units(o); });
    fuzz::check<Order>("labels fit in 8 characters", 3, 1000, [](const Order& o) { return std::format("{}-{}", o.id, o.coupon).size() <= 8; });
}

// output:
// boxes hold every unit: falsified by test 1: Order{id: 2053, coupon: "b", items: [Item{qty: 7, size: medium}, Item{qty: 13, size: medium}, Item{qty: 3, size: small}], gift: false}
// boxes hold every unit: shrunk in 5 steps to Order{id: 1000, coupon: "", items: [Item{qty: 1, size: small}], gift: false}
// fixed boxes hold every unit: passed 1000 tests
// labels fit in 8 characters: falsified by test 32: Order{id: 3451, coupon: "jsjs", items: [Item{qty: 5, size: medium}, Item{qty: 10, size: large}], gift: true}
// labels fit in 8 characters: shrunk in 8 steps to Order{id: 1000, coupon: "aaaa", items: [], gift: false}
