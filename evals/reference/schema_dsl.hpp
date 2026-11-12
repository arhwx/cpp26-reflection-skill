#pragma once

#include <algorithm>
#include <cstddef>
#include <format>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace schema_dsl {
using std::meta::info;

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

template <const char* Text>
struct object_impl;

// the struct for the fields in Text; the same text always gives the same type
template <const char* Text>
using object_t = typename object_impl<Text>::type;

constexpr bool is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
constexpr bool is_word_start(char c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
constexpr bool is_word_char(char c) { return is_word_start(c) || (c >= '0' && c <= '9'); }

// reads the fields of one object; with build set it also makes the member specs
struct parser {
    std::string_view s;
    bool build;
    std::size_t i = 0;
    std::string error;

    consteval void skip() {
        while (i < s.size() && is_space(s[i])) ++i;
    }
    consteval std::string word() {
        std::size_t start = i;
        while (i < s.size() && is_word_char(s[i])) ++i;
        return std::string(s.substr(start, i - start));
    }
    consteval bool fail(std::string message) {
        error = message;
        return false;
    }

    consteval bool fields(bool nested, std::vector<info>& specs) {
        std::vector<std::string> names;
        while (true) {
            skip();
            if (i == s.size()) return nested ? fail("missing '}'") : true;
            if (nested && s[i] == '}') {
                ++i;
                return true;
            }
            if (!is_word_start(s[i])) return fail(std::string("expected a field name, found '") + s[i] + "'");
            std::string name = word();
            if (std::ranges::contains(names, name)) return fail("duplicate field '" + name + "'");
            names.push_back(name);
            skip();
            if (i == s.size() || s[i] != ':') return fail("expected ':' after '" + name + "'");
            ++i;
            info t;
            if (!type(name, t)) return false;
            if (build) specs.push_back(data_member_spec(t, {.name = name}));
        }
    }

    consteval bool type(const std::string& field, info& out) {
        skip();
        if (i < s.size() && s[i] == '[') {
            ++i;
            info element;
            if (!type(field, element)) return false;
            skip();
            if (i == s.size() || s[i] != ']') return fail("missing ']'");
            ++i;
            if (build) out = substitute(^^std::vector, {element});
        } else if (i < s.size() && s[i] == '{') {
            std::size_t start = ++i;
            std::vector<info> unused;
            parser inner{s.substr(start), false};
            if (!inner.fields(true, unused)) return fail(inner.error);
            i = start + inner.i;
            if (build) {
                const char* text = std::define_static_string(s.substr(start, inner.i - 1));
                out = dealias(substitute(^^object_t, {std::meta::reflect_constant(text)}));
            }
        } else {
            std::string w = word();
            if (w.empty()) return fail("missing type for '" + field + "'");
            if (w == "int") out = ^^int;
            else if (w == "double") out = ^^double;
            else if (w == "bool") out = ^^bool;
            else if (w == "string") out = ^^std::string;
            else return fail("unknown type '" + w + "'");
        }
        skip();
        if (i < s.size() && s[i] == '?') {
            ++i;
            if (build) out = substitute(^^std::optional, {out});
        }
        return true;
    }
};

// "ok", or the first error in text
consteval std::string_view problem(std::string_view text) {
    parser p{text, false};
    std::vector<info> unused;
    if (p.fields(false, unused)) return "ok";
    return std::define_static_string(p.error);
}

template <const char* Text>
struct object_impl {
    static_assert(problem(Text) == "ok", problem(Text));
    struct type;
    consteval {
        parser p{Text, true};
        std::vector<info> specs;
        p.fields(false, specs);
        define_aggregate(^^type, specs);
    }
};

template <fixed_string Text>
using schema = object_t<std::define_static_string(Text.view())>;

template <class V>
struct is_vector : std::false_type {};
template <class V>
struct is_vector<std::vector<V>> : std::true_type {};
template <class V>
struct is_optional : std::false_type {};
template <class V>
struct is_optional<std::optional<V>> : std::true_type {};

template <class V>
void write(std::string& out, const V& v) {
    if constexpr (std::is_same_v<V, std::string>) {
        out += '"' + v + '"';
    } else if constexpr (is_vector<V>::value) {
        out += '[';
        for (std::size_t k = 0; k < v.size(); ++k) {
            if (k) out += ", ";
            write<typename V::value_type>(out, v[k]);  // vector<bool> elements are proxies
        }
        out += ']';
    } else if constexpr (is_optional<V>::value) {
        if (v) write(out, *v);
        else out += "null";
    } else if constexpr (std::is_class_v<V>) {
        out += '{';
        bool first = true;
        template for (constexpr info m :
                      std::define_static_array(nonstatic_data_members_of(^^V, std::meta::access_context::unprivileged()))) {
            out += first ? "" : ", ";
            out += identifier_of(m);
            out += ": ";
            write(out, v.[:m:]);
            first = false;
        }
        out += '}';
    } else {
        out += std::format("{}", v);
    }
}

template <class V>
std::string to_text(const V& v) {
    std::string out;
    write(out, v);
    return out;
}
}  // namespace schema_dsl
