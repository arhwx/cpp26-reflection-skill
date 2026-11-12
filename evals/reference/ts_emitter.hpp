#pragma once

#include <array>
#include <map>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace ts {

namespace detail {

using std::meta::info;

constexpr auto ctx = std::meta::access_context::unprivileged();

consteval info clean(info t) {
    return std::meta::dealias(std::meta::remove_cv(std::meta::dealias(t)));
}

consteval bool is_string(info t) {
    return t == clean(^^std::string) || t == clean(^^std::string_view);
}

consteval bool is_spec_of(info t, info tmpl) {
    return std::meta::has_template_arguments(t) && std::meta::template_of(t) == tmpl;
}

consteval std::vector<info> args_of(info t) {
    return std::meta::template_arguments_of(t);
}

// the kinds of type that become a typescript union
consteval bool is_union(info t) {
    t = clean(t);
    return is_spec_of(t, ^^std::optional) || (is_spec_of(t, ^^std::variant) && args_of(t).size() > 1);
}

consteval std::string expr(info t) {
    t = clean(t);
    if (t == ^^bool)
        return "boolean";
    if (std::meta::is_integral_type(t) && std::meta::size_of(t) == 8)
        return "bigint";
    if (std::meta::is_arithmetic_type(t))
        return "number";
    if (is_string(t))
        return "string";
    if (t == ^^std::monostate)
        return "null";
    if (is_spec_of(t, ^^std::optional))
        return expr(args_of(t)[0]) + " | null";
    if (is_spec_of(t, ^^std::vector) || is_spec_of(t, ^^std::array)) {
        info element = args_of(t)[0];
        return is_union(element) ? "(" + expr(element) + ")[]" : expr(element) + "[]";
    }
    if (is_spec_of(t, ^^std::map))
        return "Record<string, " + expr(args_of(t)[1]) + ">";
    if (is_spec_of(t, ^^std::variant)) {
        std::string out;
        for (info a : args_of(t))
            out += (out.empty() ? "" : " | ") + expr(a);
        return out;
    }
    if (is_spec_of(t, ^^std::tuple) || is_spec_of(t, ^^std::pair)) {
        std::string out = "[";
        bool first = true;
        for (info a : args_of(t)) {
            out += (first ? "" : ", ") + expr(a);
            first = false;
        }
        return out + "]";
    }
    if (std::meta::is_enum_type(t) || std::meta::is_class_type(t))
        return std::string(std::meta::identifier_of(t));
    throw "type has no typescript equivalent";
}

class emitter {
public:
    consteval void visit(info t) {
        t = clean(t);
        if (std::meta::is_arithmetic_type(t) || is_string(t) || t == ^^std::monostate)
            return;
        if (is_spec_of(t, ^^std::optional) || is_spec_of(t, ^^std::vector) || is_spec_of(t, ^^std::array)) {
            visit(args_of(t)[0]);
            return;
        }
        if (is_spec_of(t, ^^std::map)) {
            visit(args_of(t)[1]);
            return;
        }
        if (is_spec_of(t, ^^std::variant) || is_spec_of(t, ^^std::tuple) || is_spec_of(t, ^^std::pair)) {
            for (info a : args_of(t))
                visit(a);
            return;
        }
        for (info s : seen_)
            if (s == t)
                return;
        seen_.push_back(t);
        if (std::meta::is_enum_type(t))
            add(enum_decl(t));
        else
            add(interface_decl(t));
    }

    consteval std::string text() const { return out_; }

private:
    consteval void add(const std::string& decl) {
        if (!out_.empty())
            out_ += "\n";
        out_ += decl;
    }

    consteval std::string enum_decl(info t) {
        std::string out = "export type " + std::string(std::meta::identifier_of(t)) + " =";
        bool first = true;
        for (info e : std::meta::enumerators_of(t)) {
            out += (first ? " \"" : " | \"") + std::string(std::meta::identifier_of(e)) + "\"";
            first = false;
        }
        return out + ";\n";
    }

    // dependencies are visited, and so emitted, before the interface itself
    consteval std::string interface_decl(info t) {
        std::string head = "export interface " + std::string(std::meta::identifier_of(t));
        bool first = true;
        for (info b : std::meta::bases_of(t, ctx)) {
            info base = clean(std::meta::type_of(b));
            visit(base);
            head += (first ? " extends " : ", ") + std::string(std::meta::identifier_of(base));
            first = false;
        }
        std::string body;
        for (info m : std::meta::nonstatic_data_members_of(t, ctx)) {
            info type = clean(std::meta::type_of(m));
            visit(type);
            std::string name(std::meta::identifier_of(m));
            if (is_spec_of(type, ^^std::optional))
                body += "  " + name + "?: " + expr(args_of(type)[0]) + ";\n";
            else
                body += "  " + name + ": " + expr(type) + ";\n";
        }
        return head + " {\n" + body + "}\n";
    }

    std::vector<info> seen_;
    std::string out_;
};

consteval std::string emit_all(info t) {
    emitter e;
    e.visit(t);
    return e.text();
}

}  // namespace detail

template <class T>
std::string emit() {
    static constexpr std::string_view text = std::define_static_string(detail::emit_all(^^T));
    return std::string(text);
}

template <class T>
std::string type_expr() {
    static constexpr std::string_view text = std::define_static_string(detail::expr(^^T));
    return std::string(text);
}

}  // namespace ts
