#pragma once

#include <algorithm>
#include <functional>
#include <meta>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <utility>
#include <vector>

namespace sealed {

// one visitor member: holds a callable for C, has no default constructor
template <class R, class C>
class arm {
public:
    template <class F>
        requires(!std::is_same_v<std::remove_cvref_t<F>, arm> && std::is_invocable_r_v<R, const F&, const C&>)
    arm(F f) : fn_(std::move(f)) {}

    R operator()(const C& c) const { return fn_(c); }

private:
    std::function<R(const C&)> fn_;
};

namespace detail {

using std::meta::info;

consteval std::string snake(std::string_view name) {
    std::string out;
    for (std::size_t i = 0; i < name.size(); ++i) {
        char c = name[i];
        if (c >= 'A' && c <= 'Z') {
            if (i != 0) out += '_';
            out += static_cast<char>(c - 'A' + 'a');
        } else {
            out += c;
        }
    }
    return out;
}

// classes declared directly in ns that derive from base, ordered by member name
consteval std::vector<info> cases(info ns, info base) {
    std::vector<info> out;
    for (info m : std::meta::members_of(ns, std::meta::access_context::unchecked()))
        if (std::meta::is_type(m) && !std::meta::is_type_alias(m) && std::meta::is_class_type(m) &&
            std::meta::has_identifier(m) && m != base && std::meta::is_base_of_type(base, m))
            out.push_back(m);
    std::ranges::sort(out, [](info a, info b) {
        return snake(std::meta::identifier_of(a)) < snake(std::meta::identifier_of(b));
    });
    return out;
}

template <info Ns, class Base, class R>
struct visitor_impl {
    struct type;
    consteval {
        std::vector<info> specs;
        for (info c : cases(Ns, ^^Base))
            specs.push_back(std::meta::data_member_spec(std::meta::substitute(^^arm, {^^R, c}),
                                                        {.name = snake(std::meta::identifier_of(c))}));
        std::meta::define_aggregate(^^type, specs);
    }
};

}  // namespace detail

template <std::meta::info Ns, class Base, class R>
using visitor = typename detail::visitor_impl<Ns, Base, R>::type;

// calls the member of v for the dynamic type of obj
template <class B, class V>
auto visit(const B& obj, const V& v) {
    static constexpr auto members =
        std::define_static_array(std::meta::nonstatic_data_members_of(^^V, std::meta::access_context::unchecked()));
    using R = [:std::meta::template_arguments_of(std::meta::type_of(members[0]))[0]:];
    template for (constexpr std::meta::info m : members) {
        using C = [:std::meta::template_arguments_of(std::meta::type_of(m))[1]:];
        // a static type of B rules out cases that are not derived from B
        if constexpr (std::is_base_of_v<B, C>)
            if (typeid(obj) == typeid(C))
                return static_cast<R>(v.[:m:](static_cast<const C&>(obj)));
    }
    throw std::logic_error("unhandled dynamic type");
}

}  // namespace sealed
