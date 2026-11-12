#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <format>
#include <meta>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace fuzz {

// annotation: the values an integer member, or the integer elements of a vector member, may take
struct range {
    long long lo, hi;
};

// annotation: the largest length of a string or vector member
struct max_size {
    std::size_t n;
};

// splitmix64
class rng {
public:
    explicit rng(std::uint64_t seed) : state_(seed) {}

    std::uint64_t next() {
        state_ += 0x9e3779b97f4a7c15ull;
        std::uint64_t z = state_;
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
        z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
        return z ^ (z >> 31);
    }

private:
    std::uint64_t state_;
};

namespace detail {

// what the annotations of the enclosing member say
struct options {
    range bounds{0, 100};
    std::size_t max = 3;
};

template <class T>
inline constexpr bool is_vector = false;
template <class E, class A>
inline constexpr bool is_vector<std::vector<E, A>> = true;

template <class T>
inline constexpr bool is_integer = std::is_integral_v<T> && !std::is_same_v<T, bool>;

consteval std::vector<std::meta::info> fields(std::meta::info type) {
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

consteval options options_of(std::meta::info member) {
    options o;
    for (std::meta::info a : std::meta::annotations_of(member)) {
        std::meta::info type = std::meta::remove_const(std::meta::type_of(a));
        if (type == ^^range) o.bounds = std::meta::extract<range>(a);
        if (type == ^^max_size) o.max = std::meta::extract<max_size>(a).n;
    }
    return o;
}

template <class T>
T generate(rng& r, options o) {
    if constexpr (std::is_same_v<T, bool>) {
        return r.next() % 2 == 1;
    } else if constexpr (is_integer<T>) {
        auto span = static_cast<std::uint64_t>(o.bounds.hi - o.bounds.lo) + 1;
        return static_cast<T>(o.bounds.lo + static_cast<long long>(r.next() % span));
    } else if constexpr (std::is_enum_v<T>) {
        constexpr std::size_t count = std::meta::enumerators_of(^^T).size();
        std::size_t pick = r.next() % count, i = 0;
        T out{};
        template for (constexpr std::meta::info e : std::define_static_array(std::meta::enumerators_of(^^T)))
            if (i++ == pick) out = [:e:];
        return out;
    } else if constexpr (std::is_same_v<T, std::string>) {
        std::size_t n = r.next() % (o.max + 1);
        std::string s;
        for (std::size_t i = 0; i < n; ++i) s += static_cast<char>('a' + r.next() % 26);
        return s;
    } else if constexpr (is_vector<T>) {
        std::size_t n = r.next() % (o.max + 1);
        T v;
        // the element gets the member's range, and the default length for its own strings and vectors
        for (std::size_t i = 0; i < n; ++i) v.push_back(generate<typename T::value_type>(r, {o.bounds, 3}));
        return v;
    } else {
        T out{};
        template for (constexpr std::meta::info m : std::define_static_array(fields(^^T)))
            out.[:m:] = generate<typename [:std::meta::type_of(m):]>(r, options_of(m));
        return out;
    }
}

template <class T>
void add(std::vector<T>& out, const T& current, T candidate) {
    if (candidate == current || std::ranges::find(out, candidate) != out.end()) return;
    out.push_back(std::move(candidate));
}

template <class T>
bool same(const T& a, const T& b) {
    if constexpr (std::is_class_v<T> && !std::is_same_v<T, std::string> && !is_vector<T>) {
        bool eq = true;
        template for (constexpr std::meta::info m : std::define_static_array(fields(^^T)))
            eq = eq && same(a.[:m:], b.[:m:]);
        return eq;
    } else if constexpr (is_vector<T>) {
        return std::ranges::equal(a, b, [](const auto& x, const auto& y) { return same(x, y); });
    } else {
        return a == b;
    }
}

template <class T>
struct key {
    T value;
    bool operator==(const key& o) const { return same(value, o.value); }
};

template <class T>
std::vector<T> candidates(const T& v, options o) {
    std::vector<key<T>> out;
    key<T> cur{v};
    auto push = [&](T c) { add(out, cur, key<T>{std::move(c)}); };
    if constexpr (std::is_same_v<T, bool>) {
        if (v) push(false);
    } else if constexpr (is_integer<T>) {
        long long x = static_cast<long long>(v);
        long long t = std::clamp(0ll, o.bounds.lo, o.bounds.hi);
        if (x != t) {
            push(static_cast<T>(t));
            push(static_cast<T>(x - (x - t) / 2));
            push(static_cast<T>(x > t ? x - 1 : x + 1));
        }
    } else if constexpr (std::is_enum_v<T>) {
        template for (constexpr std::meta::info e : std::define_static_array(std::meta::enumerators_of(^^T))) {
            if (v == [:e:]) break;
            push([:e:]);
        }
    } else if constexpr (std::is_same_v<T, std::string>) {
        for (std::size_t i = 0; i < v.size(); ++i) push(std::string(v).erase(i, 1));
        for (std::size_t i = 0; i < v.size(); ++i)
            if (v[i] != 'a') {
                std::string s = v;
                s[i] = 'a';
                push(s);
            }
    } else if constexpr (is_vector<T>) {
        for (std::size_t i = 0; i < v.size(); ++i) {
            T w = v;
            w.erase(w.begin() + static_cast<std::ptrdiff_t>(i));
            push(w);
        }
        for (std::size_t i = 0; i < v.size(); ++i)
            for (auto&& c : candidates(v[i], {o.bounds, 3})) {
                T w = v;
                w[i] = std::move(c);
                push(w);
            }
    } else {
        template for (constexpr std::meta::info m : std::define_static_array(fields(^^T)))
            for (auto&& c : candidates(v.[:m:], options_of(m))) {
                T w = v;
                w.[:m:] = std::move(c);
                push(w);
            }
    }
    std::vector<T> plain;
    for (auto& k : out) plain.push_back(std::move(k.value));
    return plain;
}

}  // namespace detail

template <class T>
T arbitrary(rng& r) {
    return detail::generate<T>(r, {});
}

template <class T>
std::string show(const T& v) {
    if constexpr (std::is_same_v<T, bool> || detail::is_integer<T>) {
        return std::format("{}", v);
    } else if constexpr (std::is_enum_v<T>) {
        template for (constexpr std::meta::info e : std::define_static_array(std::meta::enumerators_of(^^T)))
            if (v == [:e:]) return std::string(std::meta::identifier_of(e));
        return "?";
    } else if constexpr (std::is_same_v<T, std::string>) {
        return "\"" + v + "\"";
    } else if constexpr (detail::is_vector<T>) {
        std::string out = "[";
        for (std::size_t i = 0; i < v.size(); ++i) out += (i ? ", " : "") + show(v[i]);
        return out + "]";
    } else {
        std::string out = std::string(std::meta::identifier_of(^^T)) + "{";
        bool first = true;
        template for (constexpr std::meta::info m : std::define_static_array(detail::fields(^^T))) {
            out += std::format("{}{}: {}", first ? "" : ", ", std::meta::identifier_of(m), show(v.[:m:]));
            first = false;
        }
        return out + "}";
    }
}

template <class T>
std::vector<T> shrinks(const T& v) {
    return detail::candidates(v, {});
}

template <class T, class P>
void check(std::string_view name, std::uint64_t seed, int runs, P property) {
    rng r(seed);
    for (int i = 1; i <= runs; ++i) {
        T v = arbitrary<T>(r);
        if (property(v)) continue;
        std::println("{}: falsified by test {}: {}", name, i, show(v));
        int steps = 0;
        for (bool progress = true; progress;) {
            progress = false;
            for (T& c : shrinks(v))
                if (!property(c)) {
                    v = std::move(c);
                    ++steps;
                    progress = true;
                    break;
                }
        }
        std::println("{}: shrunk in {} steps to {}", name, steps, show(v));
        return;
    }
    std::println("{}: passed {} tests", name, runs);
}

}  // namespace fuzz
