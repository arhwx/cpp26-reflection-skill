#pragma once

#include <cstddef>
#include <meta>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace detail {

template <std::meta::info F, std::size_t... I>
int call_with(std::span<const int> args, std::index_sequence<I...>) {
    return [:F:](args[I]...);
}

}  // namespace detail

// commands must be declared before this header is included
inline std::optional<int> dispatch(std::string_view name, std::span<const int> args) {
    constexpr auto ctx = std::meta::access_context::unprivileged();
    template for (constexpr auto f : std::define_static_array(std::meta::members_of(^^commands, ctx))) {
        if constexpr (std::meta::is_function(f)) {
            constexpr std::size_t arity = std::meta::parameters_of(f).size();
            if (name == std::meta::identifier_of(f) && args.size() == arity)
                return detail::call_with<f>(args, std::make_index_sequence<arity>());
        }
    }
    return std::nullopt;
}
