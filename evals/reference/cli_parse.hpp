#pragma once

#include <charconv>
#include <cstddef>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>

namespace detail {

template <class M>
bool parse_value(std::string_view text, M& out) {
    if constexpr (std::is_same_v<M, bool>) {
        if (text != "true" && text != "false")
            return false;
        out = text == "true";
        return true;
    } else if constexpr (std::is_same_v<M, std::string>) {
        out = std::string(text);
        return true;
    } else {
        auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), out);
        return ec == std::errc{} && end == text.data() + text.size();
    }
}

} // namespace detail

template <class T>
std::optional<T> parse_args(int argc, const char* const* argv) {
    T options{};
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (!arg.starts_with("--"))
            return std::nullopt;
        arg.remove_prefix(2);
        std::size_t eq = arg.find('=');
        std::string_view name = arg.substr(0, eq);
        bool ok = false;
        template for (constexpr auto m : std::define_static_array(
                          std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unprivileged()))) {
            if (name == std::meta::identifier_of(m)) {
                if (eq != std::string_view::npos) {
                    ok = detail::parse_value(arg.substr(eq + 1), options.[:m:]);
                } else if constexpr (std::is_same_v<typename [:std::meta::type_of(m):], bool>) {
                    options.[:m:] = true;
                    ok = true;
                }
            }
        }
        if (!ok)
            return std::nullopt;
    }
    return options;
}
