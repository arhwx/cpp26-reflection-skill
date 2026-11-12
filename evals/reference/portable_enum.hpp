#pragma once

#include <string_view>
#include <version>

#ifdef __cpp_lib_reflection
#include <meta>
#endif

enum class Color : unsigned char { black, red = 4, green, blue, white = 15 };

constexpr std::string_view color_name(Color c) {
#ifdef __cpp_lib_reflection
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^Color))) {
        if (c == [:e:])
            return std::meta::identifier_of(e);
    }
    return "unknown";
#else
    switch (c) {
    case Color::black: return "black";
    case Color::red: return "red";
    case Color::green: return "green";
    case Color::blue: return "blue";
    case Color::white: return "white";
    }
    return "unknown";
#endif
}
