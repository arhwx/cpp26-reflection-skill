#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace kp {

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

namespace detail {

using std::meta::info;

struct step {
    bool is_index = false;
    std::string_view name;  // member name
    std::size_t index = 0;  // element index
    std::size_t end = 0;    // length of the path text up to and including this step
};

constexpr bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
constexpr bool is_digit(char c) { return c >= '0' && c <= '9'; }

consteval std::string decimal(std::size_t n) {
    std::string out(1, char('0' + n % 10));
    while (n /= 10) out.insert(out.begin(), char('0' + n % 10));
    return out;
}

// the steps of a well-formed path, or false for a malformed one
consteval bool parse(std::string_view p, std::vector<step>& out) {
    std::size_t i = 0;
    bool first = true;
    while (i < p.size() || first) {
        if (!first && p[i] == '[') {
            std::size_t j = i + 1, value = 0;
            while (j < p.size() && is_digit(p[j])) value = value * 10 + std::size_t(p[j++] - '0');
            if (j == i + 1 || j >= p.size() || p[j] != ']') return false;
            out.push_back({true, {}, value, j + 1});
            i = j + 1;
            continue;
        }
        if (!first) {
            if (p[i] != '.') return false;
            ++i;
        }
        std::size_t j = i;
        if (j >= p.size() || !is_alpha(p[j])) return false;
        while (j < p.size() && (is_alpha(p[j]) || is_digit(p[j]))) ++j;
        out.push_back({false, p.substr(i, j - i), 0, j});
        i = j;
        first = false;
    }
    return true;
}

consteval bool is_spec_of(info type, info tmpl) {
    return std::meta::has_template_arguments(type) && std::meta::template_of(type) == tmpl;
}

// "ok", or what is wrong with the first failing step
consteval std::string diagnose(info root, std::string_view p) {
    std::vector<step> steps;
    if (!parse(p, steps)) return "malformed path";
    info cur = root;
    std::size_t done = 0;
    for (const step& s : steps) {
        std::string prefix = std::string(std::meta::identifier_of(root)) + (done ? "." : "");
        prefix += p.substr(0, done);
        cur = std::meta::dealias(cur);
        if (s.is_index) {
            if (std::meta::is_array_type(cur)) {
                if (s.index >= std::meta::extent(cur)) return prefix + " has no element " + decimal(s.index);
                cur = std::meta::remove_extent(cur);
            } else if (is_spec_of(cur, ^^std::array)) {
                auto args = std::meta::template_arguments_of(cur);
                if (s.index >= std::meta::extract<std::size_t>(args[1]))
                    return prefix + " has no element " + decimal(s.index);
                cur = args[0];
            } else if (is_spec_of(cur, ^^std::vector)) {
                cur = std::meta::template_arguments_of(cur)[0];
            } else {
                return prefix + " cannot be indexed";
            }
        } else {
            info found{};
            if (std::meta::is_class_type(cur))
                for (info m : std::meta::nonstatic_data_members_of(cur, std::meta::access_context::unprivileged()))
                    if (std::meta::has_identifier(m) && std::meta::identifier_of(m) == s.name) found = m;
            if (found == info{}) return prefix + " has no member '" + std::string(s.name) + "'";
            cur = std::meta::type_of(found);
        }
        done = s.end;
    }
    return "ok";
}

// a step ready to apply: a member to access, or an index
struct action {
    info member;
    std::size_t index;
};

template <class Root, fixed_string P>
consteval std::vector<action> actions() {
    std::vector<step> steps;
    parse(P.view(), steps);
    std::vector<action> out;
    info cur = ^^Root;
    for (const step& s : steps) {
        cur = std::meta::dealias(cur);
        if (s.is_index) {
            out.push_back({info{}, s.index});
            cur = std::meta::is_array_type(cur) ? std::meta::remove_extent(cur)
                                                : std::meta::template_arguments_of(cur)[0];
        } else {
            for (info m : std::meta::nonstatic_data_members_of(cur, std::meta::access_context::unprivileged()))
                if (std::meta::has_identifier(m) && std::meta::identifier_of(m) == s.name) {
                    out.push_back({m, 0});
                    cur = std::meta::type_of(m);
                }
        }
    }
    return out;
}

template <class X>
inline constexpr bool is_vector = false;
template <class U, class A>
inline constexpr bool is_vector<std::vector<U, A>> = true;

template <class Root, fixed_string P, std::size_t I = 0, class X>
constexpr decltype(auto) walk(X& x) {
    static constexpr auto acts = std::define_static_array(actions<Root, P>());
    if constexpr (I == acts.size()) {
        return (x);
    } else if constexpr (acts[I].member == std::meta::info{}) {
        constexpr std::size_t index = acts[I].index;
        if constexpr (is_vector<std::remove_const_t<X>>) return walk<Root, P, I + 1>(x.at(index));
        else return walk<Root, P, I + 1>(x[index]);
    } else {
        return walk<Root, P, I + 1>(x.[:acts[I].member:]);
    }
}

}  // namespace detail

template <class Root, fixed_string P>
consteval std::string_view check() {
    return std::define_static_string(detail::diagnose(^^Root, P.view()));
}

template <class Root, fixed_string P>
    requires(check<Root, P>() == std::string_view("ok"))
using type = std::remove_reference_t<decltype(detail::walk<Root, P>(std::declval<Root&>()))>;

template <fixed_string P, class T>
    requires(check<std::remove_const_t<T>, P>() == std::string_view("ok"))
constexpr decltype(auto) get(T& obj) {
    return detail::walk<std::remove_const_t<T>, P>(obj);
}

template <fixed_string P, class T, class V>
    requires(check<T, P>() == std::string_view("ok"))
constexpr void set(T& obj, V&& value) {
    detail::walk<T, P>(obj) = std::forward<V>(value);
}

template <fixed_string P, class T, class Fn>
    requires(check<T, P>() == std::string_view("ok"))
constexpr T over(const T& obj, Fn fn) {
    T copy = obj;
    auto& target = detail::walk<T, P>(copy);
    target = fn(std::as_const(target));
    return copy;
}

}  // namespace kp
