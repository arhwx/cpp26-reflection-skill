#pragma once

#include <bit>
#include <meta>
#include <string>
#include <type_traits>

// only single-bit enumerators are flags, so none = 0 and combined masks are skipped
template <class E>
    requires std::is_enum_v<E>
std::string flags_to_string(E value) {
    using U = std::make_unsigned_t<std::underlying_type_t<E>>;
    const U bits = static_cast<U>(value);
    std::string out;
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        constexpr U flag = static_cast<U>([:e:]);
        if constexpr (std::has_single_bit(flag)) {
            if (bits & flag) {
                if (!out.empty())
                    out += '|';
                out += std::meta::identifier_of(e);
            }
        }
    }
    return out.empty() ? "none" : out;
}
