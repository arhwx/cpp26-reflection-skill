#pragma once

#include <meta>
#include <tuple>
#include <vector>

consteval std::meta::info filter_integral(std::meta::info tuple) {
    std::vector<std::meta::info> kept;
    for (std::meta::info t : std::meta::template_arguments_of(tuple))
        if (std::meta::is_integral_type(t))
            kept.push_back(t);
    return std::meta::substitute(^^std::tuple, kept);
}

template <class Tuple>
using filter_integral_t = typename [:filter_integral(^^Tuple):];
