#pragma once

#include <meta>
#include <optional>
#include <string_view>
#include <vector>

namespace detail {

template <class T>
struct patch_builder {
    struct type;
    consteval {
        std::vector<std::meta::info> members;
        for (std::meta::info m :
             std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unprivileged())) {
            std::meta::info field = std::meta::substitute(^^std::optional, {std::meta::type_of(m)});
            members.push_back(std::meta::data_member_spec(field, {.name = std::meta::identifier_of(m)}));
        }
        std::meta::define_aggregate(^^type, members);
    }
};

consteval std::meta::info member_named(std::meta::info type, std::string_view name) {
    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged()))
        if (std::meta::identifier_of(m) == name)
            return m;
    return {};
}

} // namespace detail

// a class template member keeps one patch type per T, however often patch_of<T> is named
template <class T>
using patch_of = typename detail::patch_builder<T>::type;

template <class T>
void apply_patch(T& target, const patch_of<T>& patch) {
    template for (constexpr auto m : std::define_static_array(
                      std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unprivileged()))) {
        constexpr std::meta::info entry = detail::member_named(^^patch_of<T>, std::meta::identifier_of(m));
        if (patch.[:entry:])
            target.[:m:] = *patch.[:entry:];
    }
}
