#pragma once

#include <meta>
#include <span>
#include <string_view>
#include <vector>

namespace web {

// annotation values must be structural, so the path is copied into static storage
struct route {
    const char* path;
    consteval route(const char* p) : path(std::define_static_string(std::string_view(p))) {}
};

namespace detail {

// paths of all route annotations on a public non-static member function, empty for anything else
consteval std::vector<const char*> route_paths(std::meta::info m) {
    std::vector<const char*> paths;
    if (!std::meta::is_function(m) || std::meta::is_static_member(m) || std::meta::is_special_member_function(m))
        return paths;
    for (std::meta::info a : std::meta::annotations_of(m))
        // class-type annotations have type const route, hence remove_const
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^route)
            paths.push_back(std::meta::extract<route>(a).path);
    return paths;
}

}  // namespace detail

template <class Handler>
int handle(Handler& h, std::string_view path) {
    constexpr auto ctx = std::meta::access_context::unprivileged();
    template for (constexpr auto m : std::define_static_array(std::meta::members_of(^^Handler, ctx))) {
        constexpr std::span<const char* const> paths = std::define_static_array(detail::route_paths(m));
        if constexpr (!paths.empty()) {
            for (std::string_view p : paths)
                if (p == path)
                    return h.[:m:]();
        }
    }
    return -1;
}

}  // namespace web
