#pragma once

#include <meta>
#include <string>
#include <string_view>

namespace detail {

consteval std::string decimal(int value) {
    if (value < 0)
        return "-" + decimal(-value);
    std::string digits(1, static_cast<char>('0' + value % 10));
    return value < 10 ? digits : decimal(value / 10) + digits;
}

consteval std::string type_name_of(std::meta::info type);

consteval std::string argument_name(std::meta::info arg) {
    if (std::meta::is_type(arg))
        return type_name_of(arg);
    return decimal(std::meta::extract<int>(arg));
}

consteval std::string type_name_of(std::meta::info type) {
    // aliases name the type they stand for
    type = std::meta::dealias(type);
    // fundamental types and specializations have no identifier
    if (type == ^^int)
        return "int";
    if (type == ^^unsigned)
        return "unsigned";
    if (type == ^^long)
        return "long";
    if (type == ^^bool)
        return "bool";
    if (type == ^^char)
        return "char";
    if (type == ^^double)
        return "double";
    if (std::meta::has_template_arguments(type)) {
        std::string out(std::meta::identifier_of(std::meta::template_of(type)));
        out += '<';
        bool first = true;
        for (std::meta::info arg : std::meta::template_arguments_of(type)) {
            if (!first)
                out += ", ";
            first = false;
            out += argument_name(arg);
        }
        out += '>';
        return out;
    }
    return std::string(std::meta::identifier_of(type));
}

}  // namespace detail

template <class T>
std::string type_name() {
    constexpr std::string_view name = std::define_static_string(detail::type_name_of(^^T));
    return std::string(name);
}
