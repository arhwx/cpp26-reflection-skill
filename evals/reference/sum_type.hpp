#pragma once

#include <cstddef>
#include <functional>
#include <meta>
#include <ranges>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace adt {
using std::meta::info;

consteval std::vector<info> alts(info spec) { return nonstatic_data_members_of(spec, std::meta::access_context::unprivileged()); }

// std::variant of the alternative types, in order
consteval info storage_for(info spec) {
    std::vector<info> types;
    for (info a : alts(spec)) types.push_back(type_of(a));
    return substitute(^^std::variant, types);
}

template <class Spec>
class sum;

// an empty callable named after alternative i; calling it builds the sum
template <class Spec, std::size_t I>
struct ctor {
    using payload = [:type_of(alts(^^Spec)[I]):];
    constexpr sum<Spec> operator()(payload value = {}) const { return sum<Spec>(std::in_place_index<I>, std::move(value)); }
};

// generates struct type { <member type from wrap> <name of alternative>; ... };
template <class Spec, info Wrap, class... Extra>
struct per_alternative {
    struct type;
    consteval {
        std::vector<info> specs;
        for (std::size_t i = 0; info a : alts(^^Spec)) {
            info arg = Wrap == ^^ctor ? std::meta::reflect_constant(i++) : type_of(a);
            std::vector<info> args{^^Extra..., arg};
            specs.push_back(data_member_spec(substitute(Wrap, args), {.name = identifier_of(a)}));
        }
        define_aggregate(^^type, specs);
    }
};

// no default constructor, so every arm is required
template <class R, class A>
using handler = std::function_ref<R(const A&)>;
template <class Spec>
using make = per_alternative<Spec, ^^ctor, Spec>::type;
template <class Spec, class R>
using arms = per_alternative<Spec, ^^handler, R>::type;

template <class Spec>
class sum {
    typename [:storage_for(^^Spec):] value;
    static constexpr auto names = std::define_static_array(alts(^^Spec));

public:
    template <std::size_t I, class A>
    constexpr sum(std::in_place_index_t<I> tag, A&& a) : value(tag, std::forward<A>(a)) {}

    template <class R>
    R match(arms<Spec, R> on) const {
        static constexpr auto hs = std::define_static_array(alts(^^arms<Spec, R>));
        template for (constexpr std::size_t i : std::define_static_array(std::views::iota(0uz, names.size()))) {
            if (value.index() == i) return on.[:hs[i]:](*std::get_if<i>(&value));
        }
        std::unreachable();
    }

    // name of the active alternative
    std::string_view name() const {
        std::string_view out;
        template for (constexpr std::size_t i : std::define_static_array(std::views::iota(0uz, names.size()))) {
            if (value.index() == i) out = identifier_of(names[i]);
        }
        return out;
    }
};
}  // namespace adt
