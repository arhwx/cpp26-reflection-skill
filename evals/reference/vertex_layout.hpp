#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <meta>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace vtx {

enum class component { i8, u8, i16, u16, i32, u32, f32, f64 };

struct attribute {
    std::string_view name;
    unsigned location;
    std::size_t offset;
    unsigned count;
    component type;
    bool normalized;
};

// annotation values
struct location {
    unsigned index;
};

struct normalized_t {};
inline constexpr normalized_t normalized{};

struct skip_t {};
inline constexpr skip_t skip{};

namespace detail {

using std::meta::info;

constexpr auto ctx = std::meta::access_context::unprivileged();

// a reflection of the type itself, not of an alias or using-declaration naming it
template <class T>
constexpr info type = ^^T;

// the annotation of type A on r, or a null reflection
template <class A>
consteval info annotation(info r) {
    for (info a : std::meta::annotations_of(r))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^A)
            return a;
    return {};
}

template <class A>
consteval bool has(info r) {
    return annotation<A>(r) != info{};
}

// the explicit location of m, or fallback
consteval unsigned location_of(info m, unsigned fallback) {
    info a = annotation<location>(m);
    return a == info{} ? fallback : std::meta::extract<location>(a).index;
}

struct shape {
    bool ok;
    std::size_t count;
    component type;
};

// the component type of a scalar, or false when it is not one
consteval bool scalar(info t, component& out) {
    t = std::meta::dealias(t);
    const std::array<std::pair<info, component>, 8> table{{
        {type<std::int8_t>, component::i8},
        {type<std::uint8_t>, component::u8},
        {type<std::int16_t>, component::i16},
        {type<std::uint16_t>, component::u16},
        {type<std::int32_t>, component::i32},
        {type<std::uint32_t>, component::u32},
        {type<float>, component::f32},
        {type<double>, component::f64},
    }};
    for (auto [type, c] : table)
        if (type == t) {
            out = c;
            return true;
        }
    return false;
}

consteval shape shape_of(info t) {
    t = std::meta::dealias(std::meta::remove_cv(std::meta::dealias(t)));
    component c{};
    if (scalar(t, c))
        return {true, 1, c};
    if (std::meta::is_array_type(t) && std::meta::rank(t) == 1 && scalar(std::meta::remove_extent(t), c))
        return {true, std::meta::extent(t), c};
    if (std::meta::has_template_arguments(t) && std::meta::template_of(t) == ^^std::array) {
        auto args = std::meta::template_arguments_of(t);
        if (scalar(args[0], c))
            return {true, std::meta::extract<std::size_t>(args[1]), c};
        return {false, 0, c};
    }
    if (std::meta::is_class_type(t)) {
        // a class whose members are all one scalar type, packed without padding
        auto members = std::meta::nonstatic_data_members_of(t, ctx);
        if (members.empty() || !scalar(std::meta::type_of(members[0]), c))
            return {false, 0, c};
        info first = std::meta::dealias(std::meta::type_of(members[0]));
        for (info m : members)
            if (std::meta::dealias(std::meta::type_of(m)) != first)
                return {false, 0, c};
        if (std::meta::size_of(t) != members.size() * std::meta::size_of(first))
            return {false, 0, c};
        return {true, members.size(), c};
    }
    return {false, 0, c};
}

consteval std::string decimal(std::size_t n) {
    std::string digits;
    do {
        digits.insert(digits.begin(), static_cast<char>('0' + n % 10));
        n /= 10;
    } while (n > 0);
    return digits;
}

consteval std::vector<info> attribute_members(info type) {
    std::vector<info> out;
    for (info m : std::meta::nonstatic_data_members_of(type, ctx))
        if (!has<skip_t>(m))
            out.push_back(m);
    return out;
}

consteval std::string check(info type) {
    std::vector<unsigned> used;
    unsigned next = 0;
    for (info m : attribute_members(type)) {
        std::string prefix = std::string(std::meta::identifier_of(type)) + "." + std::string(std::meta::identifier_of(m)) + ": ";
        shape s = shape_of(std::meta::type_of(m));
        if (!s.ok)
            return prefix + "unsupported type";
        if (s.count < 1 || s.count > 4)
            return prefix + decimal(s.count) + " components, need 1 to 4";
        unsigned loc = location_of(m, next);
        for (unsigned u : used)
            if (u == loc)
                return prefix + "location " + decimal(loc) + " already used";
        used.push_back(loc);
        next = loc + 1;
    }
    return "";
}

template <class T>
consteval auto make_layout() {
    static_assert(check(^^T).empty(), "the vertex type has no valid layout");
    constexpr std::size_t n = attribute_members(^^T).size();
    std::array<attribute, n> out{};
    unsigned next = 0;
    std::size_t i = 0;
    for (info m : attribute_members(^^T)) {
        shape s = shape_of(std::meta::type_of(m));
        unsigned loc = location_of(m, next);
        next = loc + 1;
        out[i++] = attribute{
            .name = std::meta::identifier_of(m),
            .location = loc,
            .offset = static_cast<std::size_t>(std::meta::offset_of(m).bytes),
            .count = static_cast<unsigned>(s.count),
            .type = s.type,
            .normalized = has<normalized_t>(m),
        };
    }
    return out;
}

}  // namespace detail

template <class T>
inline constexpr auto layout = detail::make_layout<T>();

template <class T>
consteval std::string_view layout_error() {
    return std::define_static_string(detail::check(^^T));
}

}  // namespace vtx
