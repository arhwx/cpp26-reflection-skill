#pragma once

#include <format>
#include <meta>
#include <string>

namespace detail {

template <class T>
void append_members(std::string& out, const T& value, bool& first) {
    constexpr auto ctx = std::meta::access_context::unprivileged();
    template for (constexpr auto base : std::define_static_array(std::meta::bases_of(^^T, ctx))) {
        using Base = typename [:std::meta::type_of(base):];
        append_members(out, static_cast<const Base&>(value), first);
    }
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
        if (!first)
            out += ", ";
        first = false;
        out += std::meta::identifier_of(m);
        out += '=';
        out += std::format("{}", value.[:m:]);
    }
}

} // namespace detail

template <class T>
std::string to_string(const T& value) {
    std::string out = "{";
    bool first = true;
    detail::append_members(out, value, first);
    out += '}';
    return out;
}
