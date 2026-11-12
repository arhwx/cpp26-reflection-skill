#pragma once

#include <cstddef>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace undo {
using std::meta::info;

template <class M>
struct change {
    M before;
    M after;
};

template <class T>
struct patch_impl;

template <class T>
using patch = typename patch_impl<T>::type;

consteval std::vector<info> fields(info type) {
    return nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

consteval bool is_template_of(info type, info tmpl) {
    type = dealias(type);
    return has_template_arguments(type) && template_of(type) == tmpl;
}

// a struct member is diffed member by member; strings, vectors and scalars are leaves
consteval bool is_nested(info type) {
    type = dealias(remove_cv(type));
    return is_class_type(type) && !is_template_of(type, ^^std::basic_string) && !is_template_of(type, ^^std::vector);
}

// the member of type named like member
consteval info peer(info type, info member) {
    for (info m : fields(type))
        if (identifier_of(m) == identifier_of(member)) return m;
    return {};
}

template <class T>
struct patch_impl {
    struct type;
    consteval {
        std::vector<info> specs;
        for (info m : fields(^^T)) {
            info t = remove_cv(type_of(m));
            info member_type = is_nested(t) ? dealias(substitute(^^patch, {t}))
                                            : substitute(^^std::optional, {substitute(^^change, {t})});
            specs.push_back(data_member_spec(member_type, {.name = identifier_of(m)}));
        }
        define_aggregate(^^type, specs);
    }
};

template <class T>
patch<T> diff(const T& a, const T& b) {
    patch<T> p{};
    template for (constexpr info m : std::define_static_array(fields(^^T))) {
        constexpr info pm = peer(^^patch<T>, m);
        if constexpr (is_nested(type_of(m))) p.[:pm:] = diff(a.[:m:], b.[:m:]);
        else if (!(a.[:m:] == b.[:m:])) p.[:pm:].emplace(a.[:m:], b.[:m:]);
    }
    return p;
}

template <class T>
void apply(T& object, const patch<T>& p) {
    template for (constexpr info m : std::define_static_array(fields(^^T))) {
        constexpr info pm = peer(^^patch<T>, m);
        if constexpr (is_nested(type_of(m))) apply(object.[:m:], p.[:pm:]);
        else if (p.[:pm:]) object.[:m:] = p.[:pm:]->after;
    }
}

template <class T>
bool can_apply(const T& object, const patch<T>& p) {
    bool ok = true;
    template for (constexpr info m : std::define_static_array(fields(^^T))) {
        constexpr info pm = peer(^^patch<T>, m);
        if constexpr (is_nested(type_of(m))) ok = ok && can_apply(object.[:m:], p.[:pm:]);
        else if (p.[:pm:] && !(object.[:m:] == p.[:pm:]->before)) ok = false;
    }
    return ok;
}

template <class P>
P invert(const P& p) {
    P out{};
    template for (constexpr info m : std::define_static_array(fields(^^P))) {
        if constexpr (is_template_of(type_of(m), ^^std::optional)) {
            if (p.[:m:]) out.[:m:].emplace(p.[:m:]->after, p.[:m:]->before);
        } else out.[:m:] = invert(p.[:m:]);
    }
    return out;
}

// first then second as one patch; a leaf that ends where it started is left out
template <class P>
P merge(const P& first, const P& second) {
    P out{};
    template for (constexpr info m : std::define_static_array(fields(^^P))) {
        if constexpr (is_template_of(type_of(m), ^^std::optional)) {
            const auto& a = first.[:m:];
            const auto& b = second.[:m:];
            if (a && b) {
                if (!(a->before == b->after)) out.[:m:].emplace(a->before, b->after);
            } else if (a) out.[:m:] = a;
            else if (b) out.[:m:] = b;
        } else out.[:m:] = merge(first.[:m:], second.[:m:]);
    }
    return out;
}

template <class P>
std::size_t count(const P& p) {
    std::size_t n = 0;
    template for (constexpr info m : std::define_static_array(fields(^^P))) {
        if constexpr (is_template_of(type_of(m), ^^std::optional)) n += p.[:m:].has_value();
        else n += count(p.[:m:]);
    }
    return n;
}
}  // namespace undo
