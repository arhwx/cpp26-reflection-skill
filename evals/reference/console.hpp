#pragma once

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <format>
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

namespace console {
namespace detail {

using std::meta::info;

// variables and functions directly in the namespace whose names don't start with '_',
// sorted by name and then by parameter count
consteval std::vector<info> entries(info ns) {
    std::vector<info> out;
    for (info m : std::meta::members_of(ns, std::meta::access_context::unchecked())) {
        if (!(std::meta::is_variable(m) || std::meta::is_function(m)) || !std::meta::has_identifier(m))
            continue;
        if (std::meta::identifier_of(m).starts_with('_'))
            continue;
        out.push_back(m);
    }
    auto key = [](info m) {
        std::size_t params = std::meta::is_function(m) ? std::meta::parameters_of(m).size() : 0;
        return std::pair{std::meta::identifier_of(m), params};
    };
    std::ranges::sort(out, [&](info a, info b) { return key(a) < key(b); });
    return out;
}

consteval std::string type_name(info type) {
    type = std::meta::dealias(std::meta::remove_cvref(type));
    if (type == ^^void)
        return "void";
    if (type == ^^int)
        return "int";
    if (type == ^^double)
        return "double";
    if (type == ^^bool)
        return "bool";
    if (std::meta::is_same_type(type, ^^std::string))
        return "string";
    return std::string(std::meta::identifier_of(type));
}

consteval const char* signature(info fn) {
    std::string out(std::meta::identifier_of(fn));
    out += '(';
    bool first = true;
    for (info p : std::meta::parameters_of(fn)) {
        if (!first)
            out += ", ";
        first = false;
        out += type_name(std::meta::type_of(p)) + " " + std::string(std::meta::identifier_of(p));
    }
    out += ") -> " + type_name(std::meta::return_type_of(fn));
    return std::define_static_string(out);
}

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
    } else if constexpr (std::is_enum_v<T>) {
        template for (constexpr info e : std::define_static_array(std::meta::enumerators_of(^^T))) {
            if (text == std::meta::identifier_of(e))
                return [:e:];
        }
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

template <class T>
std::string show(const T& value) {
    if constexpr (std::is_same_v<T, std::string>) {
        return "\"" + value + "\"";
    } else if constexpr (std::is_enum_v<T>) {
        template for (constexpr info e : std::define_static_array(std::meta::enumerators_of(^^T))) {
            if (value == [:e:])
                return std::string(std::meta::identifier_of(e));
        }
        return std::to_string(std::to_underlying(value));
    } else {
        return std::format("{}", value);
    }
}

// splits on spaces; a token starting with '"' runs to the next '"'
inline std::optional<std::vector<std::string>> tokenize(std::string_view line) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < line.size()) {
        if (line[i] == ' ') {
            ++i;
        } else if (line[i] == '"') {
            std::size_t close = line.find('"', i + 1);
            if (close == std::string_view::npos)
                return std::nullopt;
            out.emplace_back(line.substr(i + 1, close - i - 1));
            i = close + 1;
        } else {
            std::size_t end = std::min(line.find(' ', i), line.size());
            out.emplace_back(line.substr(i, end - i));
            i = end;
        }
    }
    return out;
}

template <info V>
std::string variable(const std::vector<std::string>& tokens) {
    using T = std::remove_cv_t<typename [:std::meta::type_of(V):]>;
    constexpr std::string_view name = std::meta::identifier_of(V);
    if (tokens.size() > 2)
        return std::format("error: {} takes one value", name);
    if (tokens.size() == 2) {
        if constexpr (std::meta::is_const_type(std::meta::type_of(V))) {
            return std::format("error: {} is read-only", name);
        } else {
            std::optional<T> value = parse<T>(tokens[1]);
            if (!value)
                return std::format("error: bad value '{}' for {}", tokens[1], name);
            [:V:] = *std::move(value);
        }
    }
    return std::format("{} = {}", name, show([:V:]));
}

template <info Fn, std::size_t I>
using param_type = std::remove_cvref_t<typename [:std::meta::type_of(std::meta::parameters_of(Fn)[I]):]>;

template <info Fn, std::size_t... I>
std::string call(const std::vector<std::string>& tokens, std::index_sequence<I...>) {
    std::tuple<std::optional<param_type<Fn, I>>...> values{parse<param_type<Fn, I>>(tokens[I + 1])...};
    std::string error;
    template for (constexpr std::size_t i : {I...}) {
        constexpr std::string_view param = std::meta::identifier_of(std::meta::parameters_of(Fn)[i]);
        if (error.empty() && !std::get<i>(values))
            error = std::format("error: bad argument '{}' for {}", tokens[i + 1], param);
    }
    if (!error.empty())
        return error;
    if constexpr (std::meta::return_type_of(Fn) == ^^void) {
        [:Fn:](*std::move(std::get<I>(values))...);
        return "ok";
    } else {
        return show([:Fn:](*std::move(std::get<I>(values))...));
    }
}

}  // namespace detail

template <std::meta::info Ns>
std::string run(std::string_view line) {
    std::optional<std::vector<std::string>> tokens = detail::tokenize(line);
    if (!tokens)
        return "error: unterminated quote";
    if (tokens->empty())
        return "";
    const std::string& name = tokens->front();
    std::size_t argc = tokens->size() - 1;
    bool known = false;
    template for (constexpr std::meta::info m : std::define_static_array(detail::entries(Ns))) {
        if (name == std::meta::identifier_of(m)) {
            known = true;
            if constexpr (std::meta::is_variable(m)) {
                return detail::variable<m>(*tokens);
            } else {
                constexpr std::size_t n = std::meta::parameters_of(m).size();
                if (argc == n)
                    return detail::call<m>(*tokens, std::make_index_sequence<n>{});
            }
        }
    }
    if (known)
        return std::format("error: {} does not take {} arguments", name, argc);
    return std::format("error: unknown command '{}'", name);
}

template <std::meta::info Ns>
std::string help() {
    std::string out;
    template for (constexpr std::meta::info m : std::define_static_array(detail::entries(Ns))) {
        if (!out.empty())
            out += '\n';
        if constexpr (std::meta::is_variable(m)) {
            constexpr const char* type = std::define_static_string(detail::type_name(std::meta::type_of(m)));
            out += std::format("{}: {} = {}", std::meta::identifier_of(m), type, detail::show([:m:]));
            if constexpr (std::meta::is_const_type(std::meta::type_of(m)))
                out += " (read-only)";
        } else {
            out += detail::signature(m);
        }
    }
    return out;
}

}  // namespace console
