#pragma once

#include <cstddef>
#include <format>
#include <meta>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace glsl {

// a string literal as a template argument
template <std::size_t N>
struct names {
    char s[N]{};
    consteval names(const char (&a)[N]) {
        for (std::size_t i = 0; i < N; ++i) s[i] = a[i];
    }
    static constexpr std::size_t size = N - 1;
    constexpr std::string_view view() const { return {s, N - 1}; }
};

template <class T, names Ns>
struct vec;

namespace detail {

// one real data member per letter
template <class T, names Ns>
struct storage {
    struct type;
    consteval {
        std::vector<std::meta::info> specs;
        for (std::size_t i = 0; i < Ns.size; ++i)
            specs.push_back(std::meta::data_member_spec(^^T, {.name = std::string_view(&Ns.s[i], 1)}));
        std::meta::define_aggregate(^^type, specs);
    }
};

template <names Ns, names S>
consteval bool letters_known() {
    if (S.size == 0 || S.size > Ns.size) return false;
    for (char c : S.view())
        if (Ns.view().find(c) == std::string_view::npos) return false;
    return true;
}

template <names S>
consteval bool letters_distinct() {
    for (std::size_t i = 0; i < S.size; ++i)
        for (std::size_t j = i + 1; j < S.size; ++j)
            if (S.s[i] == S.s[j]) return false;
    return true;
}

template <class V>
inline constexpr bool is_vec = false;
template <class T, names Ns>
inline constexpr bool is_vec<vec<T, Ns>> = true;

}  // namespace detail

template <class T, names Ns>
struct vec : detail::storage<T, Ns>::type {
    static constexpr std::size_t size = Ns.size;
    static constexpr auto members = std::define_static_array(std::meta::nonstatic_data_members_of(
        ^^typename detail::storage<T, Ns>::type, std::meta::access_context::current()));

    constexpr T& operator[](std::size_t i) {
        template for (constexpr std::size_t k : std::define_static_array(std::views::iota(0uz, size)))
            if (i == k) return this->[:members[k]:];
        std::unreachable();
    }
    constexpr const T& operator[](std::size_t i) const { return const_cast<vec&>(*this)[i]; }

    // the first k letters of this vector's names
    template <std::size_t K>
    static consteval names<K + 1> prefix() {
        char buf[K + 1]{};
        for (std::size_t i = 0; i < K; ++i) buf[i] = Ns.s[i];
        return names<K + 1>(buf);
    }

    template <names S>
        requires(detail::letters_known<Ns, S>())
    constexpr auto sw() const {
        if constexpr (S.size == 1) {
            return this->[:members[Ns.view().find(S.s[0])]:];
        } else {
            using out_t = vec<T, prefix<S.size>()>;
            out_t out{};
            template for (constexpr std::size_t k : std::define_static_array(std::views::iota(0uz, S.size))) {
                constexpr std::size_t src = Ns.view().find(S.s[k]);
                out.[:out_t::members[k]:] = this->[:members[src]:];
            }
            return out;
        }
    }

    template <names S, names O>
        requires(detail::letters_known<Ns, S>() && detail::letters_distinct<S>() && O.size == S.size)
    constexpr vec& assign(vec<T, O> from) {
        template for (constexpr std::size_t k : std::define_static_array(std::views::iota(0uz, S.size))) {
            constexpr std::size_t dst = Ns.view().find(S.s[k]);
            this->[:members[dst]:] = from.[:vec<T, O>::members[k]:];
        }
        return *this;
    }

    friend constexpr vec operator+(vec a, const vec& b) {
        template for (constexpr auto m : members) a.[:m:] += b.[:m:];
        return a;
    }
    friend constexpr vec operator-(vec a, const vec& b) {
        template for (constexpr auto m : members) a.[:m:] -= b.[:m:];
        return a;
    }
    friend constexpr vec operator*(vec a, T k) {
        template for (constexpr auto m : members) a.[:m:] *= k;
        return a;
    }
    friend constexpr vec operator*(T k, vec a) { return a * k; }
    friend constexpr bool operator==(const vec& a, const vec& b) {
        template for (constexpr auto m : members)
            if (!(a.[:m:] == b.[:m:])) return false;
        return true;
    }
    friend constexpr T dot(const vec& a, const vec& b) {
        T t{};
        template for (constexpr auto m : members) t += a.[:m:] * b.[:m:];
        return t;
    }
    friend std::string str(const vec& v) {
        std::string out = "{";
        template for (constexpr std::size_t k : std::define_static_array(std::views::iota(0uz, size))) {
            constexpr std::string_view name = std::meta::identifier_of(members[k]);
            if (k != 0) out += ", ";
            out += std::format("{}: {}", name, v.[:members[k]:]);
        }
        return out + "}";
    }
};

}  // namespace glsl
