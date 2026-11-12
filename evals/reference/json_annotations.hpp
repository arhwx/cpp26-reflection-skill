#pragma once

#include <format>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>

namespace json {

// annotation values must be structural, so the name is copied into static storage
struct rename {
    const char* name;
    consteval rename(const char* n) : name(std::define_static_string(std::string_view(n))) {}
};

struct skip_t {};
inline constexpr skip_t skip{};

namespace detail {

// class-type annotations have type const A, hence remove_const
template <class A>
consteval std::meta::info find_annotation(std::meta::info item) {
    for (std::meta::info a : std::meta::annotations_of(item))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^A)
            return a;
    return {};
}

template <class T>
void write_value(std::string& out, const T& value) {
    if constexpr (std::is_convertible_v<const T&, std::string_view>) {
        out += '"';
        out += value;
        out += '"';
    } else {
        out += std::format("{}", value);
    }
}

} // namespace detail

template <class T>
std::string to_json(const T& value) {
    std::string out = "{";
    bool first = true;
    template for (constexpr auto m : std::define_static_array(
                      std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unprivileged()))) {
        constexpr bool skipped = detail::find_annotation<skip_t>(m) != std::meta::info{};
        if constexpr (!skipped) {
            constexpr std::meta::info renamed = detail::find_annotation<rename>(m);
            std::string_view key = std::meta::identifier_of(m);
            if constexpr (renamed != std::meta::info{})
                key = std::meta::extract<rename>(renamed).name;
            if (!first)
                out += ',';
            first = false;
            out += '"';
            out += key;
            out += "\":";
            detail::write_value(out, value.[:m:]);
        }
    }
    out += '}';
    return out;
}

} // namespace json
