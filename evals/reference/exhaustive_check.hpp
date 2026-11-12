#pragma once

#include <cstddef>
#include <format>
#include <meta>
#include <print>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace small {
using std::meta::info;

struct range {  // annotation: the values to try for an integer member
    int low, high;
};

namespace detail {
consteval std::vector<info> fields(info type) {
    return nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}
consteval range bounds_of(info member) {
    for (info a : annotations_of(member))
        if (remove_const(type_of(a)) == ^^range) return extract<range>(a);
    return {0, 2};
}
consteval std::size_t count_of(info type, range bounds) {
    type = remove_cv(type);
    if (type == ^^bool) return 2;
    if (is_enum_type(type)) return enumerators_of(type).size();
    if (is_integral_type(type)) return static_cast<std::size_t>(bounds.high - bounds.low + 1);
    std::size_t n = 1;
    for (info m : fields(type)) n *= count_of(type_of(m), bounds_of(m));
    return n;
}

template <class T>
std::vector<T> all(range bounds);

// every value for member I and, for each of them, every value for the members after it
template <class T, std::size_t I>
void fill(T& partial, std::vector<T>& out) {
    constexpr auto members = std::define_static_array(fields(^^T));
    if constexpr (I == members.size()) {
        out.push_back(partial);
    } else {
        constexpr info m = members[I];
        for (const auto& v : all<typename [:type_of(m):]>(bounds_of(m))) {
            partial.[:m:] = v;
            fill<T, I + 1>(partial, out);
        }
    }
}

template <class T>
std::vector<T> all(range bounds) {
    std::vector<T> out;
    if constexpr (std::is_same_v<T, bool>) {
        out = {false, true};
    } else if constexpr (std::is_enum_v<T>) {
        template for (constexpr info e : std::define_static_array(enumerators_of(^^T))) out.push_back([:e:]);
    } else if constexpr (std::is_integral_v<T>) {
        for (int v = bounds.low; v <= bounds.high; ++v) out.push_back(static_cast<T>(v));
    } else {
        T partial{};
        fill<T, 0>(partial, out);
    }
    return out;
}
}  // namespace detail

template <class T>
constexpr std::size_t count = detail::count_of(^^T, {0, 2});

template <class T>
std::vector<T> values() {
    return detail::all<T>({0, 2});
}

template <class T>
std::string show(const T& v) {
    if constexpr (std::is_same_v<T, bool>) {
        return v ? "true" : "false";
    } else if constexpr (std::is_enum_v<T>) {
        template for (constexpr info e : std::define_static_array(enumerators_of(^^T)))
            if (v == [:e:]) return std::string(std::meta::identifier_of(e));
        return "?";
    } else if constexpr (std::is_class_v<T>) {
        std::string out = std::string(std::meta::identifier_of(^^T)) + "{";
        template for (constexpr info m : std::define_static_array(detail::fields(^^T)))
            out += std::format("{}{}: {}", out.back() == '{' ? "" : ", ", std::meta::identifier_of(m), show(v.[:m:]));
        return out + "}";
    } else {
        return std::format("{}", v);
    }
}

template <class T>
void check(std::string_view claim, auto property) {
    std::size_t cases = 0;
    for (const T& v : values<T>())
        if (++cases; !property(v)) return std::println("{}: broken by case {}, {}", claim, cases, show(v));
    std::println("{}: holds in all {} cases", claim, cases);
}
}  // namespace small
