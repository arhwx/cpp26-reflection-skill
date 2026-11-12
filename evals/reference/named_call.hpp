#pragma once

#include <charconv>
#include <cstddef>
#include <map>
#include <meta>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace detail {

template <std::meta::info Fn>
inline constexpr auto params = std::define_static_array(std::meta::parameters_of(Fn));

// only valid for parameters that have a name
template <std::meta::info Fn, std::size_t I>
inline constexpr std::string_view param_name = std::meta::identifier_of(params<Fn>[I]);

// parameters are taken by value or by const reference, so strip both
template <std::meta::info Fn, std::size_t I>
using param_type = std::remove_cvref_t<typename [:std::meta::type_of(params<Fn>[I]):]>;

template <std::meta::info Fn>
using result_type = [:std::meta::return_type_of(Fn):];

template <class T>
std::optional<T> parse(const std::string& text) {
    if constexpr (std::is_same_v<T, std::string>) {
        return text;
    } else if constexpr (std::is_same_v<T, bool>) {
        if (text == "true")
            return true;
        if (text == "false")
            return false;
        return std::nullopt;
    } else {
        T value{};
        const char* end = text.data() + text.size();
        auto [ptr, ec] = std::from_chars(text.data(), end, value);
        if (ec != std::errc{} || ptr != end)
            return std::nullopt;
        return value;
    }
}

// a call with only the first k arguments works when those parameters have names
// and all later ones have default arguments
consteval bool can_call_with(std::meta::info fn, std::size_t k) {
    std::vector<std::meta::info> ps = std::meta::parameters_of(fn);
    for (std::size_t i = 0; i < k; ++i)
        if (!std::meta::has_identifier(ps[i]))
            return false;
    return k == ps.size() || std::meta::has_default_argument(ps[k]);
}

// calls Fn with its first sizeof...(I) parameters read from args, the rest keep their defaults
template <std::meta::info Fn, std::size_t... I>
std::optional<result_type<Fn>> call_first(const std::map<std::string, std::string>& args, std::index_sequence<I...>) {
    std::tuple<std::optional<param_type<Fn, I>>...> values{
        parse<param_type<Fn, I>>(args.find(std::string(param_name<Fn, I>))->second)...};
    if (!(std::get<I>(values).has_value() && ...))
        return std::nullopt;
    return [:Fn:](*std::move(std::get<I>(values))...);
}

}  // namespace detail

template <std::meta::info Fn>
std::optional<detail::result_type<Fn>> call_named(const std::map<std::string, std::string>& args) {
    constexpr std::size_t n = detail::params<Fn>.size();

    // count the parameters present in args, and the leading run of them;
    // a parameter without a name can't be given, so it counts as missing
    std::size_t found = 0;
    std::size_t given = 0;
    template for (constexpr std::size_t i : std::define_static_array(std::views::iota(std::size_t{0}, n))) {
        if constexpr (std::meta::has_identifier(detail::params<Fn>[i])) {
            if (args.contains(std::string(detail::param_name<Fn, i>))) {
                ++found;
                if (given == i)
                    ++given;
            }
        }
    }
    // an unknown key, or a parameter given after a missing one
    if (found != args.size() || found != given)
        return std::nullopt;

    // pass the leading run; only possible call shapes are instantiated,
    // so a missing parameter without a default leaves result empty
    std::optional<detail::result_type<Fn>> result;
    template for (constexpr std::size_t k : std::define_static_array(std::views::iota(std::size_t{0}, n + 1))) {
        if constexpr (detail::can_call_with(Fn, k)) {
            if (given == k)
                result = detail::call_first<Fn>(args, std::make_index_sequence<k>{});
        }
    }
    return result;
}
