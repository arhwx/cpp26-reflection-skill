#pragma once

#include <meta>
#include <string>
#include <string_view>

template <std::meta::info F>
std::string signature() {
    constexpr std::string_view name = std::meta::identifier_of(F);
    std::string out(name);
    out += '(';
    template for (constexpr auto p : std::define_static_array(std::meta::parameters_of(F))) {
        if (out.back() != '(')
            out += ", ";
        // unnamed, or named differently across declarations
        if constexpr (std::meta::has_identifier(p)) {
            constexpr std::string_view param = std::meta::identifier_of(p);
            out += param;
        } else {
            out += '_';
        }
    }
    out += ')';
    return out;
}
