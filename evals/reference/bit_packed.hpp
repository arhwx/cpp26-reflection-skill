#pragma once

#include <meta>
#include <string_view>
#include <vector>

namespace pack {

// annotation value; an aggregate with public members is structural
struct bits {
    int width;
};

namespace detail {

constexpr auto ctx = std::meta::access_context::unprivileged();

// width from a pack::bits annotation, or 0 when there is none
consteval int annotated_width(std::meta::info m) {
    for (std::meta::info a : std::meta::annotations_of(m))
        // class-type annotations have type const bits, hence remove_const
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^bits)
            return std::meta::extract<bits>(a).width;
    return 0;
}

// width of the member in packed_t: the annotation, else the width of an existing bit-field, else 0
consteval int packed_width(std::meta::info m) {
    if (int width = annotated_width(m))
        return width;
    if (std::meta::is_bit_field(m))
        return static_cast<int>(std::meta::bit_size_of(m));
    return 0;
}

template <class T>
struct packed_builder {
    struct type;
    consteval {
        std::vector<std::meta::info> specs;
        for (std::meta::info m : std::meta::nonstatic_data_members_of(^^T, ctx)) {
            std::meta::data_member_options options{.name = std::meta::identifier_of(m)};
            if (int width = packed_width(m))
                options.bit_width = width;
            specs.push_back(std::meta::data_member_spec(std::meta::type_of(m), options));
        }
        std::meta::define_aggregate(^^type, specs);
    }
};

consteval std::meta::info member_named(std::meta::info type, std::string_view name) {
    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, ctx))
        if (std::meta::identifier_of(m) == name)
            return m;
    return {};
}

}  // namespace detail

// a class template member keeps one packed type per T, however often packed_t<T> is named
template <class T>
using packed_t = typename detail::packed_builder<T>::type;

template <class T>
consteval int bit_width(std::string_view name) {
    std::meta::info m = detail::member_named(^^T, name);
    return m == std::meta::info{} ? 0 : detail::packed_width(m);
}

template <class T>
packed_t<T> pack(const T& value) {
    packed_t<T> out{};
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, detail::ctx))) {
        constexpr std::meta::info target = detail::member_named(^^packed_t<T>, std::meta::identifier_of(m));
        constexpr int width = detail::annotated_width(m);
        if constexpr (width > 0) {
            using M = typename [:std::meta::type_of(m):];
            // the low width bits set, without shifting by the full width of the type
            constexpr unsigned long long mask = ~0ull >> (64 - width);
            out.[:target:] = static_cast<M>(value.[:m:] & mask);
        } else {
            out.[:target:] = value.[:m:];
        }
    }
    return out;
}

template <class T>
T unpack(const packed_t<T>& value) {
    T out{};
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, detail::ctx))) {
        constexpr std::meta::info source = detail::member_named(^^packed_t<T>, std::meta::identifier_of(m));
        out.[:m:] = value.[:source:];
    }
    return out;
}

}  // namespace pack
