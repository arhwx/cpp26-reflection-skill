#pragma once

#include <algorithm>
#include <format>
#include <meta>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace handlers {

namespace detail {

using std::meta::info;

// functions declared directly in ns whose name starts with on_
consteval std::vector<info> candidates(info ns) {
    std::vector<info> out;
    for (info m : std::meta::members_of(ns, std::meta::access_context::unchecked()))
        if (std::meta::is_function(m) && std::meta::has_identifier(m) &&
            std::meta::identifier_of(m).starts_with("on_"))
            out.push_back(m);
    return out;
}

// enumerators whose value does not repeat an earlier enumerator's
template <class E>
consteval std::vector<info> primaries() {
    std::vector<info> out;
    for (info e : std::meta::enumerators_of(^^E)) {
        bool alias = std::ranges::any_of(
            out, [&](info p) { return std::meta::extract<E>(p) == std::meta::extract<E>(e); });
        if (!alias) out.push_back(e);
    }
    return out;
}

consteval std::string handler_name(info enumerator) {
    return "on_" + std::string(std::meta::identifier_of(enumerator));
}

consteval info handler_of(info ns, info enumerator) {
    for (info f : candidates(ns))
        if (std::meta::identifier_of(f) == handler_name(enumerator)) return f;
    return {};
}

}  // namespace detail

template <std::meta::info Ns, class E>
consteval std::string_view check() {
    using std::meta::info;
    std::vector<info> enums = detail::primaries<E>();
    std::vector<std::string> problems;
    for (info e : enums)
        if (detail::handler_of(Ns, e) == info{}) problems.push_back("missing " + detail::handler_name(e));
    std::vector<std::string> unused;
    for (info f : detail::candidates(Ns)) {
        std::string_view name = std::meta::identifier_of(f);
        if (std::ranges::none_of(enums, [&](info e) { return detail::handler_name(e) == name; }))
            unused.push_back("unused " + std::string(name));
    }
    std::ranges::sort(unused);
    problems.append_range(unused);
    if (problems.empty()) return "ok";
    std::string out;
    for (const std::string& p : problems) out += (out.empty() ? "" : "; ") + p;
    return std::define_static_string(out);
}

template <std::meta::info Ns, class E, class... Args>
    requires(check<Ns, E>() == std::string_view("ok"))
decltype(auto) dispatch(E e, Args&&... args) {
    static constexpr auto enums = std::define_static_array(detail::primaries<E>());
    using R = [:std::meta::return_type_of(detail::handler_of(Ns, enums[0])):];
    template for (constexpr std::meta::info en : enums) {
        if (e == [:en:]) return static_cast<R>([:detail::handler_of(Ns, en):](std::forward<Args>(args)...));
    }
    throw std::invalid_argument(std::format("no handler for value {}", +std::to_underlying(e)));
}

}  // namespace handlers
