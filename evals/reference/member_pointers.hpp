#pragma once

#include <array>
#include <cstddef>
#include <meta>
#include <string_view>
#include <vector>

namespace detail {

// public non-static data members of exactly type M; a bit-field has no member pointer
template <class T, class M>
consteval std::vector<std::meta::info> fields_of_type() {
    std::vector<std::meta::info> out;
    for (std::meta::info m : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unprivileged()))
        if (std::meta::is_same_type(std::meta::type_of(m), ^^M) && !std::meta::is_bit_field(m))
            out.push_back(m);
    return out;
}

}  // namespace detail

template <class T, class M>
constexpr auto member_pointers_of = [] {
    constexpr auto fields = std::define_static_array(detail::fields_of_type<T, M>());
    std::array<M T::*, fields.size()> out{};
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = std::meta::extract<M T::*>(fields[i]);
    return out;
}();

template <class T, class M>
constexpr auto member_names_of = [] {
    constexpr auto fields = std::define_static_array(detail::fields_of_type<T, M>());
    std::array<std::string_view, fields.size()> out{};
    for (std::size_t i = 0; i < out.size(); ++i)
        out[i] = std::meta::identifier_of(fields[i]);
    return out;
}();
