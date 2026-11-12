#pragma once

#include <cstddef>
#include <functional>
#include <meta>
#include <type_traits>

// unchecked: private members take part too

template <class T>
bool members_equal(const T& a, const T& b) {
    template for (constexpr auto m : std::define_static_array(
                      std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
        if (!(a.[:m:] == b.[:m:]))
            return false;
    }
    return true;
}

template <class T>
std::size_t members_hash(const T& value) {
    std::size_t seed = 0;
    template for (constexpr auto m : std::define_static_array(
                      std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked()))) {
        using M = std::remove_cv_t<typename [:std::meta::type_of(m):]>;
        seed ^= std::hash<M>{}(value.[:m:]) + 0x9e3779b97f4a7c15 + (seed << 6) + (seed >> 2);
    }
    return seed;
}
