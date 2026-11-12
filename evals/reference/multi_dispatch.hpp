#pragma once

#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <meta>
#include <print>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeindex>
#include <vector>

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

namespace multi {
using std::meta::info;

consteval std::vector<info> overloads(info ns, std::string_view name) {
    std::vector<info> out;
    for (info m : members_of(ns, std::meta::access_context::unprivileged()))
        if (is_function(m) && has_identifier(m) && identifier_of(m) == name) out.push_back(m);
    return out;
}
consteval std::vector<info> classes(info f) {  // parameter classes without const and reference
    std::vector<info> out;
    for (info p : parameters_of(f)) out.push_back(remove_cvref(type_of(p)));
    return out;
}
// true when every parameter class of f is the same as, or a base of, the matching one of g
consteval bool covers(info f, info g) {
    return std::ranges::equal(classes(f), classes(g), [](info x, info y) { return is_base_of_type(x, y); });
}

// most specific first: order by the number of strictly more specific overloads
consteval std::vector<info> ranked(info ns, std::string_view name) {
    std::vector<info> fs = overloads(ns, name), out;
    auto depth = [&](info f) { return std::ranges::count_if(fs, [&](info g) { return g != f && covers(f, g); }); };
    for (std::ptrdiff_t d = 0; out.size() < fs.size(); ++d)
        for (info f : fs)
            if (depth(f) == d) out.push_back(f);
    return out;
}

consteval std::string spell(std::string_view name, const std::vector<info>& cs) {
    std::string out(name);
    for (std::size_t i = 0; i < cs.size(); ++i) out += (i ? ", " : "(") + std::string(identifier_of(cs[i]));
    return out + ")";
}

// two unordered overloads that can match the same call need a third one for exactly that case
consteval std::string_view check(info ns, std::string_view name) {
    std::vector<info> fs = overloads(ns, name);
    for (info a : fs)
        for (info b : fs) {
            if (a == b || covers(a, b) || covers(b, a)) continue;
            std::vector<info> ca = classes(a), cb = classes(b), meet;
            for (std::size_t i = 0; i < ca.size() && i < cb.size(); ++i) {
                if (is_base_of_type(ca[i], cb[i])) meet.push_back(cb[i]);
                else if (is_base_of_type(cb[i], ca[i])) meet.push_back(ca[i]);
            }
            if (meet.size() != ca.size()) continue;  // the two can never match the same call
            if (std::ranges::none_of(fs, [&](info c) { return classes(c) == meet; }))
                return std::define_static_string("ambiguous: " + spell(name, ca) + " and " + spell(name, cb) + "; define " +
                                                 spell(name, meet));
        }
    return "";
}

template <class P, class A>
bool is_a(A& a) {
    if constexpr (std::is_base_of_v<std::remove_cvref_t<P>, A>) return true;  // known statically
    else return dynamic_cast<std::remove_reference_t<P>*>(&a) != nullptr;
}

// one overload with its declared parameter types as the pack p; a is the static type at the call
template <info F, class R, class... P>
struct method {
    template <class... A>
    static bool applies(A&... a) { return (... && is_a<P>(a)); }
    template <class... A>
    static R thunk(A&... a) { return [:F:](static_cast<P>(a)...); }
};
consteval info method_for(info f) {
    std::vector<info> args{std::meta::reflect_constant(f), return_type_of(f)};
    for (info p : parameters_of(f)) args.push_back(type_of(p));
    return substitute(^^method, args);
}

inline int scans = 0;  // how many calls had to search the overloads

template <info NS, fixed_string Name, class... A>
auto call(A&... a) {
    static_assert(check(NS, Name.view()).empty(), check(NS, Name.view()));
    static constexpr auto fs = std::define_static_array(ranked(NS, Name.view()));
    using R = [:return_type_of(fs[0]):];
    using fn = R (*)(A&...);
    static std::map<std::array<std::type_index, sizeof...(A)>, fn> table;  // memoized dispatch table
    std::array<std::type_index, sizeof...(A)> key{std::type_index(typeid(a))...};
    auto it = table.find(key);
    if (it == table.end()) {
        fn found = nullptr;
        template for (constexpr auto f : fs) {
            using M = [:method_for(f):];
            if (!found && M::applies(a...)) found = &M::template thunk<A...>;
        }
        if (!found) throw std::logic_error("no applicable method");
        ++scans;
        it = table.emplace(key, found).first;
    }
    return it->second(a...);
}
}  // namespace multi

