#pragma once

#include <array>
#include <cstddef>
#include <meta>
#include <optional>
#include <type_traits>

template <class E>
    requires std::is_enum_v<E>
inline constexpr std::size_t enum_count = std::meta::enumerators_of(^^E).size();

namespace detail {

// enumerators_of lists every enumerator in declaration order, duplicates included
template <class E>
consteval std::array<E, enum_count<E>> collect_values() {
    std::array<E, enum_count<E>> out{};
    std::size_t i = 0;
    for (std::meta::info e : std::meta::enumerators_of(^^E))
        out[i++] = std::meta::extract<E>(e);
    return out;
}

template <class E>
inline constexpr std::array<E, enum_count<E>> values = collect_values<E>();

}  // namespace detail

template <class E>
    requires std::is_enum_v<E>
constexpr std::array<E, enum_count<E>> enum_values() {
    return detail::values<E>;
}

template <class E>
    requires std::is_enum_v<E>
constexpr std::optional<std::size_t> enum_index(E value) {
    std::size_t i = 0;
    for (E e : detail::values<E>) {
        if (e == value)
            return i;
        ++i;
    }
    return std::nullopt;
}
