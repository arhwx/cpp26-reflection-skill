#pragma once

#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace ver {

// annotation values must be structural, so the name is copied into static storage
struct from {
    const char* name;
    consteval from(std::string_view n) : name(std::define_static_string(n)) {}
};

namespace detail {

using std::meta::info;

consteval std::vector<info> fields(info type) {
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

// the member's own name first, then its from annotations in order
consteval info source_of(info dst, info src) {
    std::vector<std::string_view> names{std::meta::identifier_of(dst)};
    for (info a : std::meta::annotations_of(dst))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^from)
            names.push_back(std::meta::extract<from>(a).name);
    for (std::string_view name : names)
        for (info s : fields(src))
            if (std::meta::identifier_of(s) == name)
                return s;
    return {};
}

consteval bool is_struct(info type) {
    return std::meta::is_class_type(type) && std::meta::is_aggregate_type(type);
}

consteval bool converts_directly(info src, info dst) {
    return std::meta::is_convertible_type(std::meta::add_lvalue_reference(std::meta::add_const(src)), dst);
}

// the first problem in declaration order, depth first, or an empty string
consteval std::string problem(info dst, info src, const std::string& prefix) {
    for (info d : fields(dst)) {
        std::string path = prefix + std::string(std::meta::identifier_of(d));
        info s = source_of(d, src);
        if (s == info{}) {
            if (!std::meta::has_default_member_initializer(d))
                return "no source for '" + path + "'";
            continue;
        }
        info dt = std::meta::dealias(std::meta::type_of(d));
        info st = std::meta::dealias(std::meta::type_of(s));
        if (converts_directly(st, dt))
            continue;
        if (is_struct(dt) && is_struct(st)) {
            std::string nested = problem(dt, st, path + ".");
            if (!nested.empty())
                return nested;
            continue;
        }
        return "cannot convert '" + path + "'";
    }
    return "";
}

}  // namespace detail

template <class To, class From>
consteval std::string_view check() {
    std::string p = detail::problem(^^To, ^^From, "");
    if (p.empty())
        return "ok";
    return std::define_static_string(p);
}

template <class To, class From>
    requires(check<To, From>() == std::string_view("ok"))
constexpr To convert(const From& from) {
    To to{};
    template for (constexpr std::meta::info d : std::define_static_array(detail::fields(^^To))) {
        constexpr std::meta::info s = detail::source_of(d, ^^From);
        if constexpr (s != std::meta::info{}) {
            using D = typename [:std::meta::type_of(d):];
            using S = typename [:std::meta::type_of(s):];
            if constexpr (std::is_convertible_v<const S&, D>) {
                D value = from.[:s:];
                to.[:d:] = std::move(value);
            } else {
                to.[:d:] = ver::convert<D>(from.[:s:]);
            }
        }
    }
    return to;
}

}  // namespace ver
