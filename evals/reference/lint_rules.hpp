#pragma once

#include <algorithm>
#include <cstddef>
#include <meta>
#include <string>
#include <string_view>
#include <vector>

namespace lint {
using std::meta::info;

struct max_padding {
    int bytes;
};

namespace detail {

consteval std::string number(std::size_t n) {
    std::string out;
    do out.insert(out.begin(), char('0' + n % 10));
    while (n /= 10);
    return out;
}

// namespaces and class names from the outermost one, joined by ::
consteval std::string qualified(info r) {
    std::string out(identifier_of(r));
    for (info p = parent_of(r); p != ^^::; p = parent_of(p)) out = std::string(identifier_of(p)) + "::" + out;
    return out;
}

consteval void collect(info ns, std::vector<info>& out) {
    for (info m : members_of(ns, std::meta::access_context::unchecked())) {
        if (is_namespace(m) && !is_namespace_alias(m)) collect(m, out);
        else if (is_type(m) && !is_type_alias(m) && is_class_type(m) && !is_union_type(m) && is_complete_type(m))
            out.push_back(m);
    }
}

consteval bool upper_camel(std::string_view s) {
    if (s.empty() || s[0] < 'A' || s[0] > 'Z') return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))) return false;
    return true;
}

consteval bool snake(std::string_view s) {
    if (s.empty() || s[0] < 'a' || s[0] > 'z') return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return false;
    return true;
}

// index of the layer namespace that contains r, or -1
consteval int layer_of(info r, const std::vector<info>& layers) {
    for (info p = parent_of(r); p != ^^::; p = parent_of(p))
        for (std::size_t i = 0; i < layers.size(); ++i)
            if (p == layers[i]) return int(i);
    return -1;
}

// the first class used by type t that lives in a layer above limit, or a null reflection
consteval info higher(info t, int limit, const std::vector<info>& layers) {
    t = dealias(t);
    while (is_reference_type(t) || is_pointer_type(t) || is_array_type(t) || is_const_type(t) || is_volatile_type(t)) {
        if (is_reference_type(t)) t = remove_reference(t);
        else if (is_pointer_type(t)) t = remove_pointer(t);
        else if (is_array_type(t)) t = remove_extent(t);
        else t = remove_cv(t);
    }
    if (!is_class_type(t)) return {};
    if (has_template_arguments(t)) {
        for (info a : template_arguments_of(t))
            if (is_type(a))
                if (info h = higher(a, limit, layers); h != info{}) return h;
        return {};
    }
    return layer_of(t, layers) > limit ? t : info{};
}

consteval std::vector<std::string> problems(info cls, const std::vector<info>& layers) {
    std::vector<std::string> out;
    auto ctx = std::meta::access_context::unchecked();
    if (!upper_camel(identifier_of(cls))) out.push_back("type name is not UpperCamelCase");
    for (info m : nonstatic_data_members_of(cls, ctx))
        if (!snake(identifier_of(m))) out.push_back("member " + std::string(identifier_of(m)) + " is not snake_case");

    const char* names[] = {"destructor", "copy constructor", "copy assignment", "move constructor", "move assignment"};
    bool declared[5] = {};
    for (info m : members_of(cls, ctx)) {
        if (!is_function(m) || !is_user_declared(m)) continue;
        if (is_destructor(m)) declared[0] = true;
        if (is_copy_constructor(m)) declared[1] = true;
        if (is_copy_assignment(m)) declared[2] = true;
        if (is_move_constructor(m)) declared[3] = true;
        if (is_move_assignment(m)) declared[4] = true;
    }
    std::string yes, no;
    for (int i = 0; i < 5; ++i) {
        std::string& list = declared[i] ? yes : no;
        list += (list.empty() ? "" : ", ") + std::string(names[i]);
    }
    if (!yes.empty() && !no.empty()) out.push_back("rule of five: declares " + yes + " but not " + no);

    for (info a : annotations_of(cls)) {
        if (remove_const(type_of(a)) != ^^max_padding) continue;
        std::size_t used = 0;
        for (info m : nonstatic_data_members_of(cls, ctx)) used += size_of(m);
        std::size_t pad = size_of(cls) - used;
        std::size_t limit = std::size_t(std::meta::extract<max_padding>(a).bytes);
        if (pad > limit) out.push_back("padding " + number(pad) + " bytes, limit " + number(limit));
    }

    int mine = layer_of(cls, layers);
    if (mine >= 0) {
        for (info b : bases_of(cls, ctx))
            if (info h = higher(type_of(b), mine, layers); h != info{})
                out.push_back("base uses " + qualified(h) + " from a higher layer");
        for (info m : nonstatic_data_members_of(cls, ctx))
            if (info h = higher(type_of(m), mine, layers); h != info{})
                out.push_back("member " + std::string(identifier_of(m)) + " uses " + qualified(h) + " from a higher layer");
    }
    return out;
}

}  // namespace detail

consteval std::string_view report(info ns, std::vector<info> layers) {
    std::vector<info> classes;
    detail::collect(ns, classes);
    std::ranges::sort(classes, {}, [](info c) { return detail::qualified(c); });
    std::string out;
    for (info c : classes)
        for (const std::string& p : detail::problems(c, layers))
            out += (out.empty() ? "" : "\n") + detail::qualified(c) + ": " + p;
    return std::define_static_string(out);
}

}  // namespace lint
