#pragma once

#include <meta>
#include <optional>
#include <string_view>

namespace detail {

consteval bool is_int_getter(std::meta::info m) {
    return std::meta::is_function(m) && !std::meta::is_static_member(m) && std::meta::has_identifier(m) &&
           std::meta::parameters_of(m).empty() && std::meta::return_type_of(m) == ^^int;
}

}  // namespace detail

template <class T>
std::optional<int> call_by_name(T& obj, std::string_view name) {
    constexpr auto ctx = std::meta::access_context::unprivileged();
    template for (constexpr auto m : std::define_static_array(std::meta::members_of(^^T, ctx))) {
        if constexpr (detail::is_int_getter(m)) {
            constexpr std::string_view id = std::meta::identifier_of(m);
            if (name == id)
                return obj.[:m:]();
        }
    }
    return std::nullopt;
}
