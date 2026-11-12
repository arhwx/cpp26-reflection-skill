#pragma once

#include <cstddef>
#include <meta>
#include <string_view>
#include <vector>

template <class T>
consteval std::size_t field_count() {
    return std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()).size();
}

template <class T>
std::vector<std::string_view> public_field_names() {
    std::vector<std::string_view> names;
    template for (constexpr auto m : std::define_static_array(
                      std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unprivileged()))) {
        names.push_back(std::meta::identifier_of(m));
    }
    return names;
}
