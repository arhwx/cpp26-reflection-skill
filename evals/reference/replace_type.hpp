#pragma once

#include <meta>
#include <vector>

namespace detail {

// rebuilds type with every occurrence of from replaced by to
consteval std::meta::info replace(std::meta::info type, std::meta::info from, std::meta::info to) {
    type = std::meta::dealias(type);
    if (type == from)
        return to;
    if (std::meta::is_const_type(type))
        return std::meta::add_const(replace(std::meta::remove_const(type), from, to));
    if (std::meta::is_pointer_type(type))
        return std::meta::add_pointer(replace(std::meta::remove_pointer(type), from, to));
    if (std::meta::has_template_arguments(type)) {
        std::vector<std::meta::info> args;
        for (std::meta::info arg : std::meta::template_arguments_of(type))
            // value and template arguments are kept, and type traits would reject them
            args.push_back(std::meta::is_type(arg) ? replace(arg, from, to) : arg);
        return std::meta::substitute(std::meta::template_of(type), args);
    }
    return type;
}

}  // namespace detail

template <class T, class From, class To>
using replace_t = typename [:detail::replace(^^T, std::meta::dealias(^^From), ^^To):];
