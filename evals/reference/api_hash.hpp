#pragma once

#include <algorithm>
#include <cstdint>
#include <meta>
#include <string>
#include <string_view>
#include <vector>

namespace surface {

namespace detail {

using std::meta::info;

constexpr std::string decimal(long long v) {
    unsigned long long u = v < 0 ? 0ull - static_cast<unsigned long long>(v) : static_cast<unsigned long long>(v);
    std::string s(1, char('0' + u % 10));
    while (u /= 10) s.insert(s.begin(), char('0' + u % 10));
    return v < 0 ? "-" + s : s;
}

template <info E>
inline constexpr long long enumerator_value = static_cast<long long>([:E:]);

consteval long long enumerator_number(info enumerator) {
    return std::meta::extract<long long>(
        std::meta::substitute(^^enumerator_value, {std::meta::reflect_constant(enumerator)}));
}

// name of a class, enum, namespace member or class member: relative to root when inside it,
// else qualified from the global namespace
consteval std::string name_of_entity(info r, info root) {
    std::string name(std::meta::identifier_of(r));
    for (info p = std::meta::parent_of(r); p != root && p != ^^::; p = std::meta::parent_of(p))
        name = std::string(std::meta::identifier_of(p)) + "::" + name;
    return name;
}

consteval std::string type_name(info t, info root) {
    if (std::meta::is_lvalue_reference_type(t)) return type_name(std::meta::remove_reference(t), root) + "&";
    if (std::meta::is_rvalue_reference_type(t)) return type_name(std::meta::remove_reference(t), root) + "&&";
    if (std::meta::is_const(t)) return type_name(std::meta::remove_const(t), root) + " const";
    if (std::meta::is_pointer_type(t)) return type_name(std::meta::remove_pointer(t), root) + "*";
    t = std::meta::dealias(t);
    struct fundamental {
        info type;
        const char* name;
    };
    constexpr fundamental names[] = {
        {^^void, "void"},
        {^^bool, "bool"},
        {^^char, "char"},
        {^^signed char, "signed char"},
        {^^unsigned char, "unsigned char"},
        {^^short, "short"},
        {^^unsigned short, "unsigned short"},
        {^^int, "int"},
        {^^unsigned int, "unsigned int"},
        {^^long, "long"},
        {^^unsigned long, "unsigned long"},
        {^^long long, "long long"},
        {^^unsigned long long, "unsigned long long"},
        {^^float, "float"},
        {^^double, "double"},
        {^^long double, "long double"},
    };
    for (const fundamental& f : names)
        if (f.type == t) return f.name;
    return name_of_entity(t, root);
}

consteval std::string params(info fn, info root) {
    std::string out = "(";
    bool first = true;
    for (info p : std::meta::parameters_of(fn)) {
        if (!first) out += ", ";
        first = false;
        out += type_name(std::meta::type_of(p), root);
        if (std::meta::has_default_argument(p)) out += " = ?";
    }
    return out + ")";
}

consteval std::string function_name(info fn, info root) {
    if (std::meta::is_operator_function(fn)) {
        std::string op = "operator" + std::string(std::meta::symbol_of(std::meta::operator_of(fn)));
        return name_of_entity(std::meta::parent_of(fn), root) + "::" + op;
    }
    return name_of_entity(fn, root);
}

// parameters, qualifiers, then the return type
consteval std::string signature(info fn, info root) {
    std::string out = params(fn, root);
    if (std::meta::is_class_member(fn) && !std::meta::is_static_member(fn) && std::meta::is_const(fn)) out += " const";
    if (std::meta::is_noexcept(fn)) out += " noexcept";
    return out + " -> " + type_name(std::meta::return_type_of(fn), root);
}

consteval void describe(info r, info root, std::vector<std::string>& lines);

consteval void describe_class(info cls, info root, std::vector<std::string>& lines) {
    std::string name = name_of_entity(cls, root);
    lines.push_back("class " + name);
    for (info m : std::meta::members_of(cls, std::meta::access_context::unprivileged())) {
        if (std::meta::is_nonstatic_data_member(m)) {
            lines.push_back("field " + name_of_entity(m, root) + ": " + type_name(std::meta::type_of(m), root));
        } else if (std::meta::is_function(m)) {
            if (!std::meta::is_user_declared(m) || std::meta::is_deleted(m) || std::meta::is_destructor(m)) continue;
            if (std::meta::is_constructor(m)) {
                lines.push_back("ctor " + name + params(m, root) + (std::meta::is_noexcept(m) ? " noexcept" : ""));
            } else if (std::meta::is_static_member(m)) {
                lines.push_back("fn " + function_name(m, root) + signature(m, root));
            } else {
                lines.push_back("method " + function_name(m, root) + signature(m, root));
            }
        } else {
            describe(m, root, lines);
        }
    }
}

consteval void describe(info r, info root, std::vector<std::string>& lines) {
    if (!std::meta::has_identifier(r)) return;
    if (std::meta::is_namespace(r)) {
        if (std::meta::identifier_of(r) == "detail") return;
        for (info m : std::meta::members_of(r, std::meta::access_context::unprivileged())) describe(m, root, lines);
    } else if (std::meta::is_template(r)) {
        lines.push_back("template " + name_of_entity(r, root));
    } else if (std::meta::is_type_alias(r)) {
        lines.push_back("using " + name_of_entity(r, root) + " = " + type_name(r, root));
    } else if (std::meta::is_type(r) && std::meta::is_enum_type(r)) {
        std::string name = name_of_entity(r, root);
        lines.push_back("enum " + name + ": " + type_name(std::meta::underlying_type(r), root));
        for (info e : std::meta::enumerators_of(r))
            lines.push_back("enumerator " + name + "::" + std::string(std::meta::identifier_of(e)) + " = " +
                            decimal(enumerator_number(e)));
    } else if (std::meta::is_type(r) && std::meta::is_class_type(r)) {
        describe_class(r, root, lines);
    } else if (std::meta::is_variable(r)) {
        lines.push_back("var " + name_of_entity(r, root) + ": " + type_name(std::meta::type_of(r), root));
    } else if (std::meta::is_function(r)) {
        if (std::meta::is_deleted(r)) return;
        lines.push_back("fn " + function_name(r, root) + signature(r, root));
    }
}

consteval std::string make_snapshot(info ns) {
    std::vector<std::string> lines;
    for (info m : std::meta::members_of(ns, std::meta::access_context::unprivileged())) describe(m, ns, lines);
    std::ranges::sort(lines);
    std::string text;
    for (const std::string& l : lines) text += l + "\n";
    return text;
}

constexpr std::uint64_t fnv1a(std::string_view text) {
    std::uint64_t h = 0xcbf29ce484222325ull;
    for (char c : text) {
        h ^= static_cast<unsigned char>(c);
        h *= 0x100000001b3ull;
    }
    return h;
}

}  // namespace detail

template <std::meta::info NS>
inline constexpr std::string_view snapshot = std::define_static_string(detail::make_snapshot(NS));

template <std::meta::info NS>
inline constexpr std::uint64_t hash = detail::fnv1a(snapshot<NS>);

}  // namespace surface
