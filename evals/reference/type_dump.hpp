#pragma once

#include <algorithm>
#include <cstddef>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace detail {

using std::meta::info;

template <auto V>
inline constexpr long long as_signed = static_cast<long long>(V);
template <auto V>
inline constexpr unsigned long long as_unsigned = static_cast<unsigned long long>(V);

consteval std::string decimal(unsigned long long value) {
    std::string digits;
    do {
        digits.insert(digits.begin(), static_cast<char>('0' + value % 10));
        value /= 10;
    } while (value != 0);
    return digits;
}

// a constant of integral or enum type, as decimal text
consteval std::string integer_text(info constant, info type) {
    if (std::meta::is_signed_type(type)) {
        long long v = std::meta::extract<long long>(std::meta::substitute(^^as_signed, {constant}));
        // negate in unsigned so the minimum value works too
        return v < 0 ? "-" + decimal(0ull - static_cast<unsigned long long>(v)) : decimal(static_cast<unsigned long long>(v));
    }
    return decimal(std::meta::extract<unsigned long long>(std::meta::substitute(^^as_unsigned, {constant})));
}

consteval std::string value_text(info constant) {
    info type = std::meta::dealias(std::meta::remove_cv(std::meta::type_of(constant)));
    if (type == ^^bool)
        return std::meta::extract<bool>(constant) ? "true" : "false";
    return integer_text(constant, type);
}

struct fundamental {
    info type;
    std::string_view name;
    std::string_view kind;
};

consteval std::vector<fundamental> fundamentals() {
    return {
        {^^void, "void", "void"},
        {^^decltype(nullptr), "nullptr_t", "null pointer"},
        {^^bool, "bool", "bool"},
        {^^char, "char", "character"},
        {^^wchar_t, "wchar_t", "character"},
        {^^char8_t, "char8_t", "character"},
        {^^char16_t, "char16_t", "character"},
        {^^char32_t, "char32_t", "character"},
        {^^signed char, "signed char", "signed integer"},
        {^^unsigned char, "unsigned char", "unsigned integer"},
        {^^short, "short", "signed integer"},
        {^^unsigned short, "unsigned short", "unsigned integer"},
        {^^int, "int", "signed integer"},
        {^^unsigned int, "unsigned int", "unsigned integer"},
        {^^long, "long", "signed integer"},
        {^^unsigned long, "unsigned long", "unsigned integer"},
        {^^long long, "long long", "signed integer"},
        {^^unsigned long long, "unsigned long long", "unsigned integer"},
        {^^float, "float", "floating point"},
        {^^double, "double", "floating point"},
        {^^long double, "long double", "floating point"},
    };
}

consteval std::optional<fundamental> find_fundamental(info type) {
    for (const fundamental& f : fundamentals())
        if (f.type == type)
            return f;
    return std::nullopt;
}

consteval std::string cv_suffix(info type) {
    std::string out;
    if (std::meta::is_const_type(type))
        out += " const";
    if (std::meta::is_volatile_type(type))
        out += " volatile";
    return out;
}

consteval std::string type_name(info type);

consteval std::string class_name(info type) {
    if (std::meta::has_template_arguments(type)) {
        std::string out(std::meta::identifier_of(std::meta::template_of(type)));
        out += '<';
        bool first = true;
        for (info arg : std::meta::template_arguments_of(type)) {
            if (!first)
                out += ", ";
            first = false;
            out += std::meta::is_type(arg) ? type_name(arg) : value_text(arg);
        }
        return out + '>';
    }
    if (!std::meta::has_identifier(type))
        return "(unnamed)";
    return std::string(std::meta::identifier_of(type));
}

// postfix spelling: T const, T*, T&, T[N], R(A, B)
consteval std::string type_name(info type) {
    type = std::meta::dealias(type);
    if (std::meta::is_array_type(type)) {
        std::string extent = std::meta::is_bounded_array_type(type) ? decimal(std::meta::extent(type)) : "";
        return type_name(std::meta::remove_extent(type)) + "[" + extent + "]";
    }
    if (std::meta::is_const_type(type) || std::meta::is_volatile_type(type))
        return type_name(std::meta::remove_cv(type)) + cv_suffix(type);
    if (std::meta::is_pointer_type(type))
        return type_name(std::meta::remove_pointer(type)) + "*";
    if (std::meta::is_lvalue_reference_type(type))
        return type_name(std::meta::remove_reference(type)) + "&";
    if (std::meta::is_rvalue_reference_type(type))
        return type_name(std::meta::remove_reference(type)) + "&&";
    if (std::meta::is_function_type(type)) {
        std::string out = type_name(std::meta::return_type_of(type)) + "(";
        bool first = true;
        for (info p : std::meta::parameters_of(type)) {
            if (!first)
                out += ", ";
            first = false;
            out += type_name(p);
        }
        return out + ")";
    }
    if (auto f = find_fundamental(type))
        return std::string(f->name);
    return class_name(type);
}

struct dumper {
    std::string out;
    std::vector<info> seen;

    consteval void line(std::size_t depth, std::string_view text) {
        out.append(depth * 2, ' ');
        out += text;
        out += '\n';
    }

    consteval void node(info type, std::string_view role, std::size_t depth) {
        type = std::meta::dealias(type);
        std::string head = role.empty() ? "" : std::string(role) + ": ";
        head += type_name(type) + " (";

        // a cv-qualified type is described like the type without cv
        info bare = std::meta::is_array_type(type) ? type : std::meta::remove_cv(type);
        std::string cv = bare == type ? "" : cv_suffix(type).substr(1);
        auto open = [&](std::string_view kind) {
            std::string text = head + std::string(kind);
            if (!cv.empty())
                text += ", " + cv;
            return text;
        };

        if (std::meta::is_array_type(bare)) {
            line(depth, open("array") + ")");
            node(std::meta::remove_extent(bare), "element", depth + 1);
        } else if (std::meta::is_pointer_type(bare)) {
            line(depth, open("pointer") + ")");
            node(std::meta::remove_pointer(bare), "to", depth + 1);
        } else if (std::meta::is_reference_type(bare)) {
            line(depth, open(std::meta::is_lvalue_reference_type(bare) ? "lvalue reference" : "rvalue reference") + ")");
            node(std::meta::remove_reference(bare), "to", depth + 1);
        } else if (std::meta::is_function_type(bare)) {
            line(depth, open("function") + ")");
            node(std::meta::return_type_of(bare), "returns", depth + 1);
            for (info p : std::meta::parameters_of(bare))
                node(p, "param", depth + 1);
        } else if (auto f = find_fundamental(bare)) {
            line(depth, open(f->kind) + ")");
        } else {
            bool is_enum = std::meta::is_enum_type(bare);
            std::string kind = is_enum ? (std::meta::is_scoped_enum_type(bare) ? "enum class" : "enum")
                                       : (std::meta::is_union_type(bare) ? "union" : "class");
            if (std::ranges::contains(seen, bare)) {
                line(depth, open(kind) + ", see above)");
                return;
            }
            seen.push_back(bare);
            line(depth, open(kind) + ")");
            if (is_enum) {
                node(std::meta::underlying_type(bare), "underlying", depth + 1);
                info underlying = std::meta::underlying_type(bare);
                for (info e : std::meta::enumerators_of(bare))
                    line(depth + 1, std::string(std::meta::identifier_of(e)) + " = " +
                                        integer_text(std::meta::constant_of(e), underlying));
                return;
            }
            if (std::meta::has_template_arguments(bare))
                for (info arg : std::meta::template_arguments_of(bare)) {
                    if (std::meta::is_type(arg))
                        node(arg, "arg", depth + 1);
                    else
                        line(depth + 1, "arg: " + value_text(arg));
                }
            auto ctx = std::meta::access_context::unchecked();
            for (info b : std::meta::bases_of(bare, ctx))
                node(std::meta::type_of(b), "base", depth + 1);
            for (info m : std::meta::nonstatic_data_members_of(bare, ctx))
                node(std::meta::type_of(m), "." + std::string(std::meta::identifier_of(m)), depth + 1);
        }
    }
};

consteval const char* dump(info type) {
    dumper d;
    d.node(type, "", 0);
    return std::define_static_string(d.out);
}

}  // namespace detail

template <class T>
std::string type_dump() {
    constexpr const char* text = detail::dump(^^T);
    return text;
}
