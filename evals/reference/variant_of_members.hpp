#pragma once

#include <algorithm>
#include <meta>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace detail {

// distinct member types without const, in order of first appearance
consteval std::meta::info members_variant(std::meta::info type) {
    std::vector<std::meta::info> types;
    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged())) {
        std::meta::info t = std::meta::remove_cv(std::meta::type_of(m));
        if (std::ranges::none_of(types, [t](std::meta::info u) { return std::meta::is_same_type(t, u); }))
            types.push_back(t);
    }
    // std::variant<> cannot be instantiated
    if (types.empty())
        types.push_back(^^std::monostate);
    return std::meta::substitute(^^std::variant, types);
}

}  // namespace detail

template <class T>
using members_variant_t = typename [:detail::members_variant(^^T):];

template <class T>
std::optional<members_variant_t<T>> get_member(const T& obj, [[maybe_unused]] std::string_view name) {
    template for (constexpr std::meta::info m : std::define_static_array(
                      std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unprivileged()))) {
        constexpr std::string_view id = std::meta::identifier_of(m);
        if (name == id) {
            using M = typename [:std::meta::remove_cv(std::meta::type_of(m)):];
            return members_variant_t<T>(std::in_place_type<M>, obj.[:m:]);
        }
    }
    return std::nullopt;
}
