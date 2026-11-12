#pragma once

#include <array>
#include <cstddef>
#include <meta>
#include <string_view>

template <class T>
consteval auto field_names() {
    constexpr auto ctx = std::meta::access_context::unprivileged();
    constexpr std::size_t count = std::meta::nonstatic_data_members_of(^^T, ctx).size();
    std::array<std::string_view, count> names{};
    std::size_t i = 0;
    for (std::meta::info m : std::meta::nonstatic_data_members_of(^^T, ctx))
        names[i++] = std::meta::identifier_of(m);
    return names;
}
