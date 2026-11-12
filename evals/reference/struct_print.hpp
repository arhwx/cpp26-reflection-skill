#pragma once

#include <format>
#include <meta>
#include <string>
#include <type_traits>

namespace detail {

template <class T>
void append(std::string& out, const T& value) {
    if constexpr (std::is_same_v<T, std::string>) {
        out += '"';
        out += value;
        out += '"';
    } else if constexpr (std::is_class_v<T>) {
        out += '{';
        bool first = true;
        template for (constexpr auto m : std::define_static_array(
                          std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unprivileged()))) {
            if (!first)
                out += ", ";
            first = false;
            out += std::meta::identifier_of(m);
            out += '=';
            append(out, value.[:m:]);
        }
        out += '}';
    } else {
        out += std::format("{}", value);
    }
}

} // namespace detail

template <class T>
std::string to_string(const T& value) {
    std::string out;
    detail::append(out, value);
    return out;
}
