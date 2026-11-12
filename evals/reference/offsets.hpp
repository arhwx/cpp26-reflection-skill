#pragma once

#include <array>
#include <cstddef>
#include <meta>
#include <string_view>

struct field_info {
    std::string_view name;
    std::size_t offset;
    std::size_t size;
};

template <class T>
constexpr auto member_offsets() {
    constexpr auto ctx = std::meta::access_context::unchecked();
    constexpr std::size_t count = std::meta::nonstatic_data_members_of(^^T, ctx).size();
    std::array<field_info, count> out{};
    std::size_t i = 0;
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
        out[i++] = {std::meta::identifier_of(m), std::meta::offset_of(m).bytes, std::meta::size_of(m)};
    }
    return out;
}
