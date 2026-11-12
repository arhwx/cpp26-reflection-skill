#pragma once

#include <meta>
#include <optional>
#include <string_view>
#include <type_traits>

template <class E>
    requires std::is_enum_v<E>
constexpr std::string_view enum_name(E value) {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (value == [:e:])
            return std::meta::identifier_of(e);
    }
    return {};
}

template <class E>
    requires std::is_enum_v<E>
constexpr std::optional<E> enum_from_name(std::string_view name) {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (name == std::meta::identifier_of(e))
            return [:e:];
    }
    return std::nullopt;
}
