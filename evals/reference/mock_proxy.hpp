#pragma once

#include <cstddef>
#include <format>
#include <functional>
#include <meta>
#include <print>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace mocks {

// one method of a mock: records every call and answers with the configured behavior
template <class R, class... A>
class mock_method {
public:
    using call_t = std::tuple<std::remove_cvref_t<A>...>;

    R operator()(A... args) const {
        calls_.emplace_back(args...);
        if (behavior_)
            return behavior_(args...);
        if constexpr (!std::is_void_v<R>)
            return R{};
    }

    template <class V>
    void returns(V value) {
        behavior_ = [result = R(std::move(value))](A...) -> R { return result; };
    }

    template <class F>
    void invokes(F fn) {
        behavior_ = std::move(fn);
    }

    std::size_t count() const { return calls_.size(); }
    const std::vector<call_t>& calls() const { return calls_; }

private:
    mutable std::vector<call_t> calls_;
    std::function<R(A...)> behavior_;
};

namespace detail {

using std::meta::info;

constexpr auto ctx = std::meta::access_context::unprivileged();

// named non-static member functions, in declaration order
consteval std::vector<info> methods(info type) {
    std::vector<info> out;
    for (info m : std::meta::members_of(type, ctx))
        if (std::meta::is_function(m) && std::meta::has_identifier(m) && !std::meta::is_static_member(m) &&
            !std::meta::is_constructor(m) && !std::meta::is_destructor(m) && !std::meta::is_operator_function(m))
            out.push_back(m);
    return out;
}

consteval std::vector<info> parameter_types(info m) {
    std::vector<info> out;
    for (info p : std::meta::parameters_of(m))
        out.push_back(std::meta::dealias(std::meta::type_of(p)));
    return out;
}

// return type followed by parameter types
consteval std::vector<info> signature(info m) {
    std::vector<info> out{std::meta::dealias(std::meta::return_type_of(m))};
    out.append_range(parameter_types(m));
    return out;
}

// the member function of type with the name and parameter types of interface method m
consteval info find_impl(info type, info m) {
    for (info c : methods(type))
        if (std::meta::identifier_of(c) == std::meta::identifier_of(m) && parameter_types(c) == parameter_types(m))
            return c;
    return {};
}

consteval std::vector<const char*> parameter_names(info m) {
    std::vector<const char*> out;
    std::size_t i = 0;
    for (info p : std::meta::parameters_of(m)) {
        std::string name = "arg";
        if (std::meta::has_identifier(p)) {
            name = std::meta::identifier_of(p);
        } else {
            std::string digits;
            std::size_t n = i;
            do {
                digits.insert(digits.begin(), static_cast<char>('0' + n % 10));
                n /= 10;
            } while (n > 0);
            name += digits;
        }
        out.push_back(std::define_static_string(name));
        ++i;
    }
    return out;
}

template <class V>
std::string show(const V& value) {
    using D = std::remove_cvref_t<V>;
    if constexpr (std::is_same_v<D, std::string> || std::is_same_v<D, std::string_view> ||
                  std::is_same_v<std::decay_t<D>, const char*> || std::is_same_v<std::decay_t<D>, char*>)
        return std::format("\"{}\"", value);
    else
        return std::format("{}", value);
}

// one method of a logging proxy: forwards to the object and prints the call
template <class T, info M, info Impl, class R, class... A>
struct logged_method {
    T* self;

    R operator()(A... args) const {
        if constexpr (std::is_void_v<R>) {
            self->[:Impl:](args...);
            std::println("[log] {}", describe(args...));
        } else {
            R result = self->[:Impl:](args...);
            std::println("[log] {} -> {}", describe(args...), show(result));
            return result;
        }
    }

    static std::string describe(const std::remove_reference_t<A>&... args) {
        static constexpr auto names = std::define_static_array(parameter_names(M));
        std::string out = std::string(std::meta::identifier_of(M)) + "(";
        std::size_t i = 0;
        ((out += (i == 0 ? "" : ", ") + std::string(names[i]) + "=" + show(args), ++i), ...);
        return out + ")";
    }
};

template <class I>
struct mock_builder {
    struct type;
    consteval {
        std::vector<info> specs;
        for (info m : methods(^^I))
            specs.push_back(std::meta::data_member_spec(std::meta::substitute(^^mock_method, signature(m)),
                                                        {.name = std::meta::identifier_of(m)}));
        std::meta::define_aggregate(^^type, specs);
    }
};

consteval info logged_method_for(info object, info m) {
    std::vector<info> args{object, std::meta::reflect_constant(m), std::meta::reflect_constant(find_impl(object, m))};
    args.append_range(signature(m));
    return std::meta::substitute(^^logged_method, args);
}

template <class I, class T>
struct logged_builder {
    struct type;
    consteval {
        std::vector<info> specs;
        for (info m : methods(^^I))
            specs.push_back(std::meta::data_member_spec(logged_method_for(^^T, m), {.name = std::meta::identifier_of(m)}));
        std::meta::define_aggregate(^^type, specs);
    }
};

consteval bool implements(info type, info interface) {
    for (info m : methods(interface))
        if (find_impl(type, m) == info{})
            return false;
    return true;
}

template <class I, class T, std::size_t... K>
auto bind(T& object, std::index_sequence<K...>) {
    return typename logged_builder<I, T>::type{{(static_cast<void>(K), &object)}...};
}

}  // namespace detail

template <class I>
using mock = typename detail::mock_builder<I>::type;

template <class I, class T>
auto logged(T& object) {
    static_assert(detail::implements(^^T, ^^I), "the object lacks a method of the interface");
    constexpr std::size_t n = detail::methods(^^I).size();
    return detail::bind<I>(object, std::make_index_sequence<n>{});
}

}  // namespace mocks
