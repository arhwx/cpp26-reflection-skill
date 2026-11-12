#pragma once

#include <algorithm>
#include <cstddef>
#include <format>
#include <meta>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace ts {
using std::meta::info;

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

consteval std::vector<info> fields(info type) { return nonstatic_data_members_of(type, std::meta::access_context::unprivileged()); }

// the member with that name, or a null reflection
consteval info find(info type, std::string_view name) {
    for (info m : fields(type))
        if (identifier_of(m) == name) return m;
    return {};
}

// "ok", or the error a wrong member name gives
consteval std::string_view problem(info type, std::string_view name) {
    if (find(type, name) != info{}) return "ok";
    return std::define_static_string(std::string(identifier_of(type)) + " has no member '" + std::string(name) + "'");
}

consteval bool is_optional(info type) { return has_template_arguments(dealias(type)) && template_of(dealias(type)) == ^^std::optional; }

// same member names, every member type wrapped in std::optional
template <class T>
struct partial_impl {
    struct type;
    consteval {
        std::vector<info> specs;
        for (info m : fields(^^T)) specs.push_back(data_member_spec(substitute(^^std::optional, {type_of(m)}), {.name = identifier_of(m)}));
        define_aggregate(^^type, specs);
    }
};

// the members whose names are listed (Keep) or the members whose names are not listed
template <class T, fixed_string Names, bool Keep>
struct select_impl {
    static consteval std::vector<std::string_view> listed() {
        std::vector<std::string_view> out;
        for (auto word : std::views::split(Names.view(), ' ')) out.emplace_back(word);
        return out;
    }
    static_assert(std::ranges::all_of(listed(), [](std::string_view n) { return find(^^T, n) != info{}; }), "a listed name is not a member");
    struct type;
    consteval {
        std::vector<info> specs;
        for (info m : fields(^^T))
            if (std::ranges::contains(listed(), identifier_of(m)) == Keep) specs.push_back(data_member_spec(type_of(m), {.name = identifier_of(m)}));
        define_aggregate(^^type, specs);
    }
};

template <class T> using partial = typename partial_impl<T>::type;
template <class T, fixed_string Names> using pick = typename select_impl<T, Names, true>::type;
template <class T, fixed_string Names> using omit = typename select_impl<T, Names, false>::type;

// copies every member of src that dst also has, by name. an empty optional in src leaves dst alone
template <class Dst, class Src>
constexpr void assign(Dst& dst, const Src& src) {
    template for (constexpr info d : std::define_static_array(fields(^^Dst))) {
        constexpr info s = find(^^Src, identifier_of(d));
        if constexpr (s == info{}) continue;
        else if constexpr (is_optional(type_of(s))) {
            if (src.[:s:]) {
                if constexpr (is_optional(type_of(d))) dst.[:d:] = src.[:s:];
                else dst.[:d:] = *src.[:s:];
            }
        } else dst.[:d:] = src.[:s:];
    }
}

template <class Dst, class Src>
constexpr Dst as(const Src& src) {
    Dst dst{};
    assign(dst, src);
    return dst;
}

template <class T>
constexpr T with(T base, const std::type_identity_t<partial<T>>& changes) {
    assign(base, changes);
    return base;
}

template <fixed_string Key, class T>
    requires(find(^^T, Key.view()) != info{})
constexpr auto& get(T& object) {
    return object.[:find(^^T, Key.view()):];
}

template <class T>
std::string show(const T& object) {
    std::string out = "{";
    template for (constexpr info m : std::define_static_array(fields(^^T))) {
        out += std::format("{}{}: ", out.size() > 1 ? ", " : "", std::meta::identifier_of(m));
        if constexpr (is_optional(type_of(m))) out += object.[:m:] ? std::format("{}", *object.[:m:]) : "none";
        else out += std::format("{}", object.[:m:]);
    }
    return out + "}";
}
}  // namespace ts
