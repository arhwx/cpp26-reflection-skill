#pragma once

#include <array>
#include <cstddef>
#include <format>
#include <meta>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace named {
namespace detail {

using std::meta::info;

// a member that has no default constructor, so its designated initializer is mandatory
template <class T>
struct required {
    T value;

    template <class U>
        requires std::is_convertible_v<U, T>
    constexpr required(U&& u) : value(std::forward<U>(u)) {}
};

consteval bool is_mutable_ref(info type) {
    return std::meta::is_lvalue_reference_type(type) && !std::meta::is_const_type(std::meta::remove_reference(type));
}

// T& binds the caller's object, a defaulted parameter may be left out, every other one is required
consteval info member_type(info param) {
    info type = std::meta::type_of(param);
    if (is_mutable_ref(type))
        return type;
    info value = std::meta::remove_cvref(type);
    if (std::meta::has_default_argument(param))
        return std::meta::substitute(^^std::optional, {value});
    return std::meta::substitute(^^required, {value});
}

consteval bool is_method(info fn) {
    return std::meta::is_class_member(fn) && !std::meta::is_static_member(fn);
}

template <info Fn>
struct holder {
    static constexpr auto params = std::define_static_array(std::meta::parameters_of(Fn));

    struct type;
    consteval {
        std::vector<info> specs;
        for (info p : params)
            specs.push_back(std::meta::data_member_spec(member_type(p), {.name = std::meta::identifier_of(p)}));
        std::meta::define_aggregate(^^type, specs);
    }

    static constexpr auto members =
        std::define_static_array(std::meta::nonstatic_data_members_of(^^type, std::meta::access_context::unchecked()));
    static constexpr std::size_t n = params.size();
    static constexpr auto names = [] {
        std::vector<const char*> out;
        for (info p : params)
            out.push_back(std::define_static_string(std::meta::identifier_of(p)));
        return std::define_static_array(out);
    }();
    // default arguments are trailing, so calls with fewer arguments start here
    static constexpr std::size_t first_default = [] {
        std::size_t k = n;
        while (k > 0 && std::meta::has_default_argument(params[k - 1]))
            --k;
        return k;
    }();
};

template <info Fn>
using result = [:std::meta::return_type_of(Fn):];

template <info Fn, std::size_t I, class Args>
decltype(auto) argument(Args& args) {
    constexpr info m = holder<Fn>::members[I];
    constexpr info type = std::meta::type_of(m);
    if constexpr (std::meta::is_reference_type(type))
        return args.[:m:];
    else if constexpr (std::meta::has_default_argument(holder<Fn>::params[I]))
        return std::move(*args.[:m:]);
    else
        return std::move(args.[:m:].value);
}

// which arguments the caller gave; only defaulted parameters can be left out
template <info Fn, class Args>
std::array<bool, holder<Fn>::n> present(const Args& args) {
    std::array<bool, holder<Fn>::n> out{};
    template for (constexpr std::size_t i : std::define_static_array(std::views::iota(std::size_t{0}, holder<Fn>::n))) {
        if constexpr (std::meta::has_default_argument(holder<Fn>::params[i]))
            out[i] = args.[:holder<Fn>::members[i]:].has_value();
        else
            out[i] = true;
    }
    return out;
}

// how many leading arguments to pass; throws when a later argument follows a left-out one
template <info Fn, class Args>
std::size_t leading(const Args& args) {
    constexpr std::size_t n = holder<Fn>::n;
    auto given = present<Fn>(args);
    std::size_t k = 0;
    while (k < n && given[k])
        ++k;
    for (std::size_t j = k + 1; j < n; ++j)
        if (given[j])
            throw std::invalid_argument(std::format("{}: {} must be given because {} is given",
                                                    std::meta::identifier_of(Fn), holder<Fn>::names[k],
                                                    holder<Fn>::names[j]));
    return k;
}

template <info Fn, class Obj, std::size_t... I>
result<Fn> call_first(Obj* obj, typename holder<Fn>::type& args, std::index_sequence<I...>) {
    if constexpr (is_method(Fn))
        return (obj->[:Fn:])(argument<Fn, I>(args)...);
    else
        return [:Fn:](argument<Fn, I>(args)...);
}

template <info Fn, class Obj>
result<Fn> call(Obj* obj, typename holder<Fn>::type& args) {
    std::size_t k = leading<Fn>(args);
    template for (constexpr std::size_t c :
                  std::define_static_array(std::views::iota(holder<Fn>::first_default, holder<Fn>::n + 1))) {
        if (k == c)
            return call_first<Fn>(obj, args, std::make_index_sequence<c>{});
    }
    std::unreachable();
}

}  // namespace detail

template <std::meta::info Fn>
using args = typename detail::holder<Fn>::type;

template <std::meta::info Fn>
    requires(!detail::is_method(Fn))
detail::result<Fn> invoke(args<Fn> a) {
    return detail::call<Fn>(static_cast<void*>(nullptr), a);
}

template <std::meta::info Fn, class Obj>
    requires(detail::is_method(Fn))
detail::result<Fn> invoke(Obj& obj, args<Fn> a) {
    return detail::call<Fn>(&obj, a);
}

}  // namespace named
