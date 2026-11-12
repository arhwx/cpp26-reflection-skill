#pragma once

#include <meta>
#include <string>
#include <string_view>

namespace detail {

// there is no standard qualified-name query, so walk the parents up to the global namespace
consteval std::string build_qualified_name(std::meta::info r) {
    std::string out(std::meta::identifier_of(r));
    for (std::meta::info p = std::meta::parent_of(r); p != ^^::; p = std::meta::parent_of(p))
        out = std::string(std::meta::identifier_of(p)) + "::" + out;
    return out;
}

template <std::meta::info R>
inline constexpr std::string_view qualified_name_text = std::define_static_string(build_qualified_name(R));

}  // namespace detail

template <std::meta::info R>
constexpr std::string_view qualified_name() {
    return detail::qualified_name_text<R>;
}
