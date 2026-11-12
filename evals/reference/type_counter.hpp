#pragma once

#include <cstddef>
#include <meta>
#include <type_traits>
#include <vector>

namespace ct {
using std::meta::info;

namespace detail {

// tick<Tag, N> is completed by the bump of Tag that raises its count to N + 1
template <class Tag, std::size_t N>
struct tick;

// entry<Tag, N> is completed when the N-th type joins the list of Tag, and holds that type
template <class Tag, std::size_t N>
struct entry;

consteval info slot(info tmpl, info tag, std::size_t n) {
    return std::meta::substitute(tmpl, {tag, std::meta::reflect_constant(n)});
}

// number of completed slots, the state visible at the point of evaluation
consteval std::size_t filled(info tmpl, info tag) {
    std::size_t n = 0;
    while (std::meta::is_complete_type(slot(tmpl, tag, n)))
        ++n;
    return n;
}

}  // namespace detail

consteval std::size_t count(info tag) {
    return detail::filled(^^detail::tick, tag);
}

consteval void bump(info tag) {
    std::meta::define_aggregate(detail::slot(^^detail::tick, tag, count(tag)), std::vector<info>{});
}

consteval std::vector<info> types(info tag) {
    std::vector<info> out;
    std::size_t n = detail::filled(^^detail::entry, tag);
    for (std::size_t i = 0; i < n; ++i) {
        info held = std::meta::nonstatic_data_members_of(detail::slot(^^detail::entry, tag, i),
                                                         std::meta::access_context::unchecked())[0];
        out.push_back(std::meta::template_arguments_of(std::meta::type_of(held))[0]);
    }
    return out;
}

consteval int index_of(info tag, info type) {
    std::vector<info> list = types(tag);
    for (std::size_t i = 0; i < list.size(); ++i)
        if (std::meta::is_same_type(list[i], type))
            return static_cast<int>(i);
    return -1;
}

consteval void add(info tag, info type) {
    if (index_of(tag, type) >= 0)
        return;
    info held = std::meta::data_member_spec(std::meta::substitute(^^std::type_identity, {type}), {.name = "held"});
    std::meta::define_aggregate(detail::slot(^^detail::entry, tag, types(tag).size()), {held});
}

}  // namespace ct
