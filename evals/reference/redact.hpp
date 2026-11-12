#pragma once

#include <concepts>
#include <format>
#include <meta>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace sec {

struct secret_t {};
inline constexpr secret_t secret{};

namespace detail {

using std::meta::info;

consteval std::vector<info> fields(info type) {
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

consteval bool is_secret(info member) {
    for (info a : std::meta::annotations_of(member))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^secret_t)
            return true;
    return false;
}

// a member of aggregate class type is written as a nested object
consteval bool is_nested(info type) {
    type = std::meta::dealias(type);
    return std::meta::is_class_type(type) && std::meta::is_aggregate_type(type);
}

consteval void collect(info type, const std::string& prefix, std::vector<const char*>& out) {
    for (info m : fields(type)) {
        std::string path = prefix + std::string(std::meta::identifier_of(m));
        if (is_secret(m))
            out.push_back(std::define_static_string(path));
        else if (is_nested(std::meta::type_of(m)))
            collect(std::meta::type_of(m), path + ".", out);
    }
}

consteval std::vector<const char*> paths(info type) {
    std::vector<const char*> out;
    collect(type, "", out);
    return out;
}

template <class P, class V>
concept policy_for = requires(const P& policy, std::string_view path, const V& value) {
    { policy(path, value) } -> std::convertible_to<std::string>;
};

// every secret reachable in T can be written by P
template <class T, class P>
consteval bool covers() {
    bool ok = true;
    template for (constexpr info m : std::define_static_array(fields(^^T))) {
        using M = typename [:std::meta::type_of(m):];
        if constexpr (is_secret(m))
            ok = ok && policy_for<P, M>;
        else if constexpr (is_nested(std::meta::type_of(m)))
            ok = ok && covers<M, P>();
    }
    return ok;
}

// how secrets are written: redacted, absent, or through a policy
struct redact_mode {};
struct plain_mode {};

template <class T, class Mode>
void write(std::string& out, const T& value, const Mode& mode, const std::string& path);

template <class V, class Mode>
void write_value(std::string& out, const V& value, const Mode& mode, const std::string& path) {
    if constexpr (std::is_same_v<V, std::string>) {
        out += '"';
        out += value;
        out += '"';
    } else if constexpr (is_nested(^^V)) {
        write(out, value, mode, path + ".");
    } else {
        out += std::format("{}", value);
    }
}

template <class T, class Mode>
void write(std::string& out, const T& value, const Mode& mode, const std::string& prefix) {
    out += '{';
    bool first = true;
    template for (constexpr info m : std::define_static_array(fields(^^T))) {
        if (!first)
            out += ", ";
        first = false;
        out += std::meta::identifier_of(m);
        out += ": ";
        std::string path = prefix + std::string(std::meta::identifier_of(m));
        if constexpr (!is_secret(m))
            write_value(out, value.[:m:], mode, path);
        else if constexpr (std::is_same_v<Mode, redact_mode>)
            out += "***";
        else
            out += std::string(mode(std::string_view(path), value.[:m:]));
    }
    out += '}';
}

}  // namespace detail

template <class T>
consteval std::span<const char* const> secret_paths() {
    return std::define_static_array(detail::paths(^^T));
}

template <class T>
inline constexpr bool classified = !detail::paths(^^T).empty();

template <class T>
std::string redacted(const T& value) {
    std::string out;
    detail::write(out, value, detail::redact_mode{}, "");
    return out;
}

template <class T>
    requires(!classified<T>)
std::string serialize(const T& value) {
    std::string out;
    detail::write(out, value, detail::plain_mode{}, "");
    return out;
}

template <class T, class P>
    requires(detail::covers<T, P>())
std::string serialize(const T& value, const P& policy) {
    std::string out;
    detail::write(out, value, policy, "");
    return out;
}

}  // namespace sec
