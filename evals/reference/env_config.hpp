#pragma once

#include <charconv>
#include <cstdlib>
#include <format>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace env {

// annotation values must be structural, so the text is copied into static storage
struct name {
    const char* text;
    consteval name(std::string_view s) : text(std::define_static_string(s)) {}
};

struct required_t {};
inline constexpr required_t required{};

namespace detail {

using std::meta::info;

consteval std::vector<info> fields(info type) {
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

template <class A>
consteval info find_annotation(info member) {
    for (info a : std::meta::annotations_of(member))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^A)
            return a;
    return {};
}

// the part of the variable name contributed by a member
consteval const char* segment(info member) {
    if (info a = find_annotation<name>(member); a != info{})
        return std::meta::extract<name>(a).text;
    std::string out(std::meta::identifier_of(member));
    for (char& c : out)
        if (c >= 'a' && c <= 'z')
            c = char(c - 'a' + 'A');
    return std::define_static_string(out);
}

consteval bool is_template_of(info type, info templ) {
    type = std::meta::dealias(type);
    return std::meta::has_template_arguments(type) && std::meta::template_of(type) == templ;
}

consteval info first_argument(info type) {
    return std::meta::dealias(std::meta::template_arguments_of(std::meta::dealias(type))[0]);
}

consteval bool is_scalar(info type) {
    type = std::meta::dealias(type);
    if (type == std::meta::dealias(^^std::string) || type == ^^bool || std::meta::is_enum_type(type) ||
        std::meta::is_floating_point_type(type))
        return true;
    if (!std::meta::is_integral_type(type))
        return false;
    for (info c : {^^char, ^^signed char, ^^unsigned char, ^^wchar_t, ^^char8_t, ^^char16_t, ^^char32_t})
        if (type == c)
            return false;
    return true;
}

consteval bool is_leaf(info type) {
    if (is_template_of(type, ^^std::optional) || is_template_of(type, ^^std::vector))
        return is_scalar(first_argument(type));
    return is_scalar(type);
}

consteval bool is_nested(info type) {
    type = std::meta::dealias(type);
    return std::meta::is_class_type(type) && std::meta::is_aggregate_type(type) && !is_leaf(type);
}

consteval bool supported(info type) {
    if (is_leaf(type))
        return true;
    if (!is_nested(type))
        return false;
    for (info m : fields(std::meta::dealias(type)))
        if (!supported(std::meta::type_of(m)))
            return false;
    return true;
}

template <class T>
std::optional<T> parse_scalar(std::string_view text) {
    if constexpr (std::is_same_v<T, std::string>) {
        return std::string(text);
    } else if constexpr (std::is_same_v<T, bool>) {
        if (text == "true" || text == "1" || text == "yes" || text == "on")
            return true;
        if (text == "false" || text == "0" || text == "no" || text == "off")
            return false;
        return std::nullopt;
    } else if constexpr (std::is_enum_v<T>) {
        template for (constexpr info e : std::define_static_array(std::meta::enumerators_of(^^T)))
            if (text == std::meta::identifier_of(e))
                return [:e:];
        return std::nullopt;
    } else {
        T value{};
        auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec != std::errc{} || end != text.data() + text.size())
            return std::nullopt;
        return value;
    }
}

template <class T>
std::optional<T> parse(std::string_view text) {
    if constexpr (is_template_of(^^T, ^^std::optional)) {
        using V = typename T::value_type;
        if (auto v = parse_scalar<V>(text))
            return T(std::move(*v));
        return std::nullopt;
    } else if constexpr (is_template_of(^^T, ^^std::vector)) {
        using V = typename T::value_type;
        T out;
        if (text.empty())
            return out;
        while (true) {
            std::size_t comma = text.find(',');
            auto v = parse_scalar<V>(text.substr(0, comma));
            if (!v)
                return std::nullopt;
            out.push_back(std::move(*v));
            if (comma == std::string_view::npos)
                return out;
            text.remove_prefix(comma + 1);
        }
    } else {
        return parse_scalar<T>(text);
    }
}

template <class T>
void collect(const std::string& prefix, std::vector<std::string>& out) {
    template for (constexpr info m : std::define_static_array(fields(^^T))) {
        std::string var = prefix + "_" + segment(m);
        using M = typename [:std::meta::type_of(m):];
        if constexpr (is_nested(^^M))
            collect<M>(var, out);
        else
            out.push_back(var);
    }
}

template <class T>
void load(T& config, const std::string& prefix, std::vector<std::string>& errors) {
    template for (constexpr info m : std::define_static_array(fields(^^T))) {
        std::string var = prefix + "_" + segment(m);
        using M = typename [:std::meta::type_of(m):];
        if constexpr (is_nested(^^M)) {
            load(config.[:m:], var, errors);
        } else {
            const char* raw = std::getenv(var.c_str());
            if (raw == nullptr) {
                if constexpr (find_annotation<required_t>(m) != info{})
                    errors.push_back(std::format("{}: required but not set", var));
            } else if (std::optional<M> value = parse<M>(raw)) {
                config.[:m:] = std::move(*value);
            } else {
                errors.push_back(std::format("{}: invalid value '{}'", var, raw));
            }
        }
    }
}

}  // namespace detail

template <class T>
    requires(detail::is_nested(^^T) && detail::supported(^^T))
std::vector<std::string> variables(std::string_view prefix) {
    std::vector<std::string> out;
    detail::collect<T>(std::string(prefix), out);
    return out;
}

template <class T>
    requires(detail::is_nested(^^T) && detail::supported(^^T))
std::vector<std::string> load(T& config, std::string_view prefix) {
    std::vector<std::string> errors;
    detail::load(config, std::string(prefix), errors);
    return errors;
}

}  // namespace env
