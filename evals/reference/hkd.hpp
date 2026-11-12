#pragma once

#include <cstddef>
#include <functional>
#include <meta>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace hkd {

namespace detail {

using std::meta::info;

consteval std::vector<info> fields(info type) {
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

// every member type of T wrapped in F, with the same names
template <class T, template <class> class F>
struct wrapped {
    struct type;
    consteval {
        std::vector<info> specs;
        for (info m : fields(^^T))
            specs.push_back(std::meta::data_member_spec(std::meta::substitute(^^F, {std::meta::type_of(m)}),
                                                        {.name = std::meta::identifier_of(m)}));
        std::meta::define_aggregate(^^type, specs);
    }
};

template <class T>
inline constexpr auto members_of_t = std::define_static_array(fields(^^T));

// the member of a map type with the same name as the member m of T
template <class M>
consteval info same_name(info m) {
    for (info b : fields(^^M))
        if (std::meta::identifier_of(b) == std::meta::identifier_of(m)) return b;
    return {};
}

}  // namespace detail

// a class template, not an alias, so functions can deduce T and F; the members live in the base
template <class T, template <class> class F>
struct map : detail::wrapped<T, F>::type {};

template <class T, template <class> class F, class Fn>
void for_each(map<T, F>& m, Fn fn) {
    template for (constexpr auto mem : detail::members_of_t<T>) {
        constexpr auto b = detail::same_name<typename detail::wrapped<T, F>::type>(mem);
        fn(std::string_view(std::meta::identifier_of(mem)), m.[:b:]);
    }
}

template <class T, template <class> class F, class Fn>
void for_each(const map<T, F>& m, Fn fn) {
    template for (constexpr auto mem : detail::members_of_t<T>) {
        constexpr auto b = detail::same_name<typename detail::wrapped<T, F>::type>(mem);
        fn(std::string_view(std::meta::identifier_of(mem)), m.[:b:]);
    }
}

template <template <class> class G, class T, template <class> class F, class Fn>
map<T, G> transform(const map<T, F>& m, Fn fn) {
    map<T, G> out{};
    template for (constexpr auto mem : detail::members_of_t<T>) {
        constexpr auto from = detail::same_name<typename detail::wrapped<T, F>::type>(mem);
        constexpr auto to = detail::same_name<typename detail::wrapped<T, G>::type>(mem);
        out.[:to:] = fn(m.[:from:]);
    }
    return out;
}

template <class T>
std::optional<T> sequence(const map<T, std::optional>& m) {
    T out{};
    template for (constexpr auto mem : detail::members_of_t<T>) {
        constexpr auto from = detail::same_name<typename detail::wrapped<T, std::optional>::type>(mem);
        if (!m.[:from:]) return std::nullopt;
        out.[:mem:] = *m.[:from:];
    }
    return out;
}

template <class T>
T at(const map<T, std::vector>& columns, std::size_t i) {
    T out{};
    template for (constexpr auto mem : detail::members_of_t<T>) {
        constexpr auto from = detail::same_name<typename detail::wrapped<T, std::vector>::type>(mem);
        out.[:mem:] = columns.[:from:].at(i);
    }
    return out;
}

namespace detail {
template <class T, std::size_t... I>
map<T, std::reference_wrapper> refs(T& obj, std::index_sequence<I...>) {
    return {{std::ref(obj.[:members_of_t<T>[I]:])...}};
}
}  // namespace detail

template <class T>
map<T, std::reference_wrapper> refs(T& obj) {
    return detail::refs(obj, std::make_index_sequence<detail::members_of_t<T>.size()>{});
}

}  // namespace hkd
