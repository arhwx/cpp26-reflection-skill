#pragma once

#include <cstddef>
#include <meta>
#include <tuple>
#include <utility>
#include <vector>

namespace detail {

consteval std::vector<std::meta::info> public_fields(std::meta::info type) {
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

consteval std::meta::info tuple_type(std::meta::info type) {
    std::vector<std::meta::info> types;
    for (std::meta::info m : public_fields(type))
        types.push_back(std::meta::type_of(m));
    return std::meta::substitute(^^std::tuple, types);
}

}  // namespace detail

template <class T>
using tuple_type_t = typename [:detail::tuple_type(^^T):];

template <class T>
tuple_type_t<T> to_tuple(const T& obj) {
    static constexpr auto fields = std::define_static_array(detail::public_fields(^^T));
    return [&]<std::size_t... I>(std::index_sequence<I...>) {
        return tuple_type_t<T>(obj.[:fields[I]:]...);
    }(std::make_index_sequence<fields.size()>());
}
