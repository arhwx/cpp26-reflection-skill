#pragma once

#include <array>
#include <cstddef>
#include <meta>
#include <string_view>
#include <vector>

namespace detail {

constexpr auto ctx = std::meta::access_context::unprivileged();

consteval std::size_t member_alignment(std::meta::info m) {
    return std::meta::alignment_of(std::meta::type_of(m));
}

// members by decreasing alignment; an insertion sort keeps equal alignments in declaration order
// (std::sort is not stable, and std::stable_sort does not compile for info on GCC 16 or the fork)
consteval std::vector<std::meta::info> compact_order(std::meta::info type) {
    std::vector<std::meta::info> out;
    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, ctx)) {
        auto pos = out.end();
        while (pos != out.begin() && member_alignment(*(pos - 1)) < member_alignment(m))
            --pos;
        out.insert(pos, m);
    }
    return out;
}

template <class T>
struct compact_builder {
    struct type;
    consteval {
        std::vector<std::meta::info> specs;
        for (std::meta::info m : compact_order(^^T))
            specs.push_back(std::meta::data_member_spec(std::meta::type_of(m), {.name = std::meta::identifier_of(m)}));
        std::meta::define_aggregate(^^type, specs);
    }
};

consteval std::meta::info member_named(std::meta::info type, std::string_view name) {
    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, ctx))
        if (std::meta::identifier_of(m) == name)
            return m;
    return {};
}

}  // namespace detail

// a class template member keeps one compact type per T, however often compact_t<T> is named
template <class T>
using compact_t = typename detail::compact_builder<T>::type;

template <class T>
consteval auto compact_names() {
    constexpr std::size_t count = std::meta::nonstatic_data_members_of(^^compact_t<T>, detail::ctx).size();
    std::array<std::string_view, count> names{};
    std::size_t i = 0;
    for (std::meta::info m : std::meta::nonstatic_data_members_of(^^compact_t<T>, detail::ctx))
        names[i++] = std::meta::identifier_of(m);
    return names;
}

template <class T>
compact_t<T> to_compact(const T& value) {
    compact_t<T> out{};
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, detail::ctx))) {
        constexpr std::meta::info target = detail::member_named(^^compact_t<T>, std::meta::identifier_of(m));
        out.[:target:] = value.[:m:];
    }
    return out;
}

template <class T>
T from_compact(const compact_t<T>& value) {
    T out{};
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, detail::ctx))) {
        constexpr std::meta::info source = detail::member_named(^^compact_t<T>, std::meta::identifier_of(m));
        out.[:m:] = value.[:source:];
    }
    return out;
}
