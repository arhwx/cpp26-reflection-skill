#pragma once

#include <compare>
#include <concepts>
#include <cstddef>
#include <format>
#include <functional>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace rs {

enum class trait { Debug, Eq, Ord, Hash };
inline constexpr trait Debug = trait::Debug;
inline constexpr trait Eq = trait::Eq;
inline constexpr trait Ord = trait::Ord;
inline constexpr trait Hash = trait::Hash;

// the annotation: a set of traits
struct derive {
    unsigned mask = 0;
    consteval derive(std::same_as<trait> auto... ts) : mask((0u | ... | (1u << static_cast<unsigned>(ts)))) {}
};

namespace detail {

inline constexpr trait all_traits[] = {trait::Debug, trait::Eq, trait::Ord, trait::Hash};

consteval std::string_view trait_name(trait t) {
    switch (t) {
        case trait::Debug: return "Debug";
        case trait::Eq: return "Eq";
        case trait::Ord: return "Ord";
        case trait::Hash: return "Hash";
    }
    return "?";
}

template <class T>
inline constexpr bool native_debug = std::formattable<T, char>;
template <class T>
inline constexpr bool native_eq = std::equality_comparable<T>;
template <class T>
inline constexpr bool native_ord = std::totally_ordered<T>;
template <class T>
inline constexpr bool native_hash = requires(const T& v) { std::hash<T>{}(v); };

// traits of all derive annotations on type, or -1 when it has none
consteval long derived_mask(std::meta::info type) {
    if (!std::meta::is_class_type(type) && !std::meta::is_enum_type(type)) return -1;
    long mask = -1;
    for (std::meta::info a : std::meta::annotations_of(type))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^derive)
            mask = (mask < 0 ? 0 : mask) | std::meta::extract<derive>(a).mask;
    return mask;
}

consteval bool lists(std::meta::info type, trait t) {
    long mask = derived_mask(type);
    return mask >= 0 && (mask & (1L << static_cast<unsigned>(t)));
}

consteval std::vector<std::meta::info> fields(std::meta::info type) {
    if (!std::meta::is_class_type(type)) return {};
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unchecked());
}

consteval bool implements(std::meta::info type, trait t);

// the first broken rule of a derived type, or an empty string
consteval std::string problem(std::meta::info type, trait t) {
    for (std::meta::info m : fields(type))
        if (!implements(std::meta::remove_cv(std::meta::type_of(m)), t))
            return std::string("member '") + std::string(std::meta::identifier_of(m)) +
                   "' does not implement " + std::string(trait_name(t));
    if ((t == trait::Ord || t == trait::Hash) && !lists(type, trait::Eq))
        return std::string(trait_name(t)) + " requires Eq";
    return "";
}

consteval bool implements(std::meta::info type, trait t) {
    if (derived_mask(type) < 0) {
        std::meta::info native = t == trait::Debug ? ^^native_debug
                                 : t == trait::Eq  ? ^^native_eq
                                 : t == trait::Ord ? ^^native_ord
                                                   : ^^native_hash;
        return std::meta::extract<bool>(std::meta::substitute(native, {type}));
    }
    if (!lists(type, t)) return false;
    if (!problem(type, t).empty()) return false;
    return t == trait::Eq || t == trait::Debug || implements(type, trait::Eq);
}

consteval std::string_view type_name(std::meta::info type) {
    if (std::meta::has_template_arguments(type)) return std::meta::identifier_of(std::meta::template_of(type));
    return std::meta::identifier_of(type);
}

}  // namespace detail

template <class T, trait X>
concept has = detail::implements(^^T, X);

template <class T>
consteval std::string_view check() {
    if (detail::derived_mask(^^T) >= 0)
        for (trait t : detail::all_traits)
            if (detail::lists(^^T, t))
                if (std::string p = detail::problem(^^T, t); !p.empty())
                    return std::define_static_string(std::string(detail::type_name(^^T)) + ": " + p);
    return "ok";
}

template <class T>
    requires has<T, Debug>
std::string debug(const T& value) {
    if constexpr (detail::derived_mask(^^T) < 0) {
        if constexpr (std::is_same_v<T, std::string> || std::is_same_v<T, std::string_view> ||
                      std::is_same_v<T, char>)
            return std::format("{:?}", value);
        else
            return std::format("{}", value);
    } else if constexpr (std::is_enum_v<T>) {
        template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^T)))
            if (value == [:e:]) return std::string(std::meta::identifier_of(e));
        return std::format("{}", std::to_underlying(value));
    } else {
        constexpr std::string_view name = detail::type_name(^^T);
        static constexpr auto members = std::define_static_array(detail::fields(^^T));
        std::string out(name);
        if constexpr (members.size() == 0) return out;
        out += " { ";
        bool first = true;
        template for (constexpr auto m : members) {
            constexpr std::string_view field = std::meta::identifier_of(m);
            if (!first) out += ", ";
            first = false;
            out += std::format("{}: {}", field, rs::debug(value.[:m:]));
        }
        return out + " }";
    }
}

template <class T>
    requires has<T, Eq>
bool eq(const T& a, const T& b) {
    if constexpr (detail::derived_mask(^^T) < 0 || std::is_enum_v<T>) {
        return a == b;
    } else {
        template for (constexpr auto m : std::define_static_array(detail::fields(^^T)))
            if (!rs::eq(a.[:m:], b.[:m:])) return false;
        return true;
    }
}

template <class T>
    requires has<T, Ord>
std::weak_ordering cmp(const T& a, const T& b) {
    if constexpr (detail::derived_mask(^^T) < 0 || std::is_enum_v<T>) {
        if (a < b) return std::weak_ordering::less;
        if (b < a) return std::weak_ordering::greater;
        return std::weak_ordering::equivalent;
    } else {
        std::weak_ordering result = std::weak_ordering::equivalent;
        template for (constexpr auto m : std::define_static_array(detail::fields(^^T))) {
            if (result != 0) break;
            result = rs::cmp(a.[:m:], b.[:m:]);
        }
        return result;
    }
}

template <class T>
    requires has<T, Hash>
std::size_t hash(const T& value) {
    if constexpr (detail::derived_mask(^^T) < 0 || std::is_enum_v<T>) {
        return std::hash<T>{}(value);
    } else {
        std::size_t seed = 0;
        template for (constexpr auto m : std::define_static_array(detail::fields(^^T)))
            seed ^= rs::hash(value.[:m:]) + 0x9e3779b97f4a7c15uz + (seed << 6) + (seed >> 2);
        return seed;
    }
}

}  // namespace rs
