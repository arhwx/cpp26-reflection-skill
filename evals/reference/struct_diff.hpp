#pragma once

#include <cstddef>
#include <format>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace detail {

// T is deduced from const T&, so const members arrive here without const
template <class T>
void diff_into(std::vector<std::string>& out, const std::string& path, const T& a, const T& b) {
    if constexpr (std::is_same_v<T, std::string>) {
        if (a != b)
            out.push_back(std::format("{}: \"{}\" -> \"{}\"", path, a, b));
    } else if constexpr (std::is_array_v<T>) {
        for (std::size_t i = 0; i < std::extent_v<T>; ++i)
            diff_into(out, std::format("{}[{}]", path, i), a[i], b[i]);
    } else if constexpr (std::is_class_v<T>) {
        template for (constexpr std::meta::info m : std::define_static_array(
                          std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unprivileged()))) {
            constexpr std::string_view name = std::meta::identifier_of(m);
            diff_into(out, path.empty() ? std::string(name) : std::format("{}.{}", path, name), a.[:m:], b.[:m:]);
        }
    } else {
        if (a != b)
            out.push_back(std::format("{}: {} -> {}", path, a, b));
    }
}

}  // namespace detail

template <class T>
std::vector<std::string> diff(const T& a, const T& b) {
    std::vector<std::string> out;
    detail::diff_into(out, "", a, b);
    return out;
}
