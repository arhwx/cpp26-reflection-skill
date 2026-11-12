#pragma once

#include <meta>
#include <string_view>
#include <vector>

namespace detail {

template <class T>
struct soa_builder {
    struct type;
    consteval {
        std::vector<std::meta::info> columns;
        for (std::meta::info m :
             std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unprivileged())) {
            std::meta::info column = std::meta::substitute(^^std::vector, {std::meta::type_of(m)});
            columns.push_back(std::meta::data_member_spec(column, {.name = std::meta::identifier_of(m)}));
        }
        std::meta::define_aggregate(^^type, columns);
    }
};

consteval std::meta::info member_named(std::meta::info type, std::string_view name) {
    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged()))
        if (std::meta::identifier_of(m) == name)
            return m;
    return {};
}

} // namespace detail

template <class T>
using soa_columns = typename detail::soa_builder<T>::type;

template <class T>
void push(soa_columns<T>& columns, const T& value) {
    template for (constexpr auto m : std::define_static_array(
                      std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unprivileged()))) {
        constexpr std::meta::info column =
            detail::member_named(^^typename detail::soa_builder<T>::type, std::meta::identifier_of(m));
        columns.[:column:].push_back(value.[:m:]);
    }
}
