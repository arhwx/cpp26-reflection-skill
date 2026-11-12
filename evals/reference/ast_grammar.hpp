#pragma once

#include <algorithm>
#include <cstddef>
#include <format>
#include <memory>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace peg {

// class annotation: an empty struct that stands for a fixed token
struct lit {
    const char* text;
    consteval lit(std::string_view t) : text(std::define_static_string(t)) {}
};
// member annotations: tokens before and after the member
struct prefix {
    const char* text;
    consteval prefix(std::string_view t) : text(std::define_static_string(t)) {}
};
struct suffix {
    const char* text;
    consteval suffix(std::string_view t) : text(std::define_static_string(t)) {}
};

namespace detail {
using std::meta::info;

// texts of the annotations of type A, in the order written
template <class A>
consteval std::vector<const char*> texts(info r) {
    std::vector<const char*> out;
    for (info a : std::meta::annotations_of(r))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^A) out.push_back(std::meta::extract<A>(a).text);
    return out;
}

enum class kind { integer, identifier, box, many, maybe, choice, token, rule };

consteval kind kind_of(info t) {
    t = std::meta::dealias(t);
    if (t == ^^int) return kind::integer;
    if (t == std::meta::dealias(^^std::string)) return kind::identifier;
    if (std::meta::has_template_arguments(t)) {
        info tmpl = std::meta::template_of(t);
        if (tmpl == ^^std::unique_ptr) return kind::box;
        if (tmpl == ^^std::vector) return kind::many;
        if (tmpl == ^^std::optional) return kind::maybe;
        if (tmpl == ^^std::variant) return kind::choice;
    }
    if (!texts<lit>(t).empty()) return kind::token;
    return kind::rule;
}


consteval std::vector<info> args(info t) { return std::meta::template_arguments_of(std::meta::dealias(t)); }
consteval std::vector<info> fields(info t) {
    return std::meta::nonstatic_data_members_of(t, std::meta::access_context::unprivileged());
}

// ebnf for a type used inside a rule; rule types it names are appended to refs
consteval std::string expr(info t, std::vector<info>& refs) {
    switch (kind_of(t)) {
        case kind::integer: return "integer";
        case kind::identifier: return "identifier";
        case kind::box: return expr(args(t)[0], refs);
        case kind::many: return "{ " + expr(args(t)[0], refs) + " }";
        case kind::maybe: return "[ " + expr(args(t)[0], refs) + " ]";
        case kind::choice: {
            std::string out = "( ";
            bool first = true;
            for (info a : args(t)) {
                out += (first ? "" : " | ") + expr(a, refs);
                first = false;
            }
            return out + " )";
        }
        case kind::token: return "\"" + std::string(texts<lit>(std::meta::dealias(t))[0]) + "\"";
        case kind::rule: break;
    }
    refs.push_back(std::meta::dealias(t));
    return std::string(std::meta::identifier_of(std::meta::dealias(t)));
}

consteval std::string rule_body(info t, std::vector<info>& refs) {
    std::string out;
    auto add = [&](std::string item) { out += (out.empty() ? "" : " ") + item; };
    for (info m : fields(t)) {
        for (const char* p : texts<prefix>(m)) add("\"" + std::string(p) + "\"");
        add(expr(std::meta::type_of(m), refs));
        for (const char* s : texts<suffix>(m)) add("\"" + std::string(s) + "\"");
    }
    return out;
}

// root first, then rules in the order they are first named
consteval std::string_view grammar_text(info root) {
    std::vector<info> rules{root};
    std::string out;
    for (std::size_t i = 0; i < rules.size(); ++i) {
        std::vector<info> refs;
        out += std::string(std::meta::identifier_of(rules[i])) + " = " + rule_body(rules[i], refs) + " ;\n";
        for (info r : refs)
            if (std::ranges::find(rules, r) == rules.end()) rules.push_back(r);
    }
    return std::define_static_string(out);
}

struct cursor {
    std::string_view text;
    std::size_t pos = 0;

    void skip_space() {
        while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\n')) ++pos;
    }
    bool literal(std::string_view s) {
        skip_space();
        if (!text.substr(pos).starts_with(s)) return false;
        pos += s.size();
        return true;
    }
};

constexpr bool is_alpha(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
constexpr bool is_digit(char c) { return c >= '0' && c <= '9'; }

// on failure the position is unspecified; callers that try alternatives restore it
template <class T>
bool read(cursor& in, T& out) {
    constexpr kind k = kind_of(^^T);
    if constexpr (k == kind::integer) {
        in.skip_space();
        std::size_t start = in.pos;
        int v = 0;
        while (in.pos < in.text.size() && is_digit(in.text[in.pos])) v = v * 10 + (in.text[in.pos++] - '0');
        out = v;
        return in.pos > start;
    } else if constexpr (k == kind::identifier) {
        in.skip_space();
        std::size_t start = in.pos;
        if (in.pos >= in.text.size() || !is_alpha(in.text[in.pos])) return false;
        while (in.pos < in.text.size() && (is_alpha(in.text[in.pos]) || is_digit(in.text[in.pos]))) ++in.pos;
        out = std::string(in.text.substr(start, in.pos - start));
        return true;
    } else if constexpr (k == kind::box) {
        using X = typename T::element_type;
        X value;
        if (!read(in, value)) return false;
        out = std::make_unique<X>(std::move(value));
        return true;
    } else if constexpr (k == kind::many) {
        using X = typename T::value_type;
        while (true) {
            std::size_t start = in.pos;
            X value;
            if (!read(in, value)) {
                in.pos = start;
                return true;
            }
            out.push_back(std::move(value));
        }
    } else if constexpr (k == kind::maybe) {
        using X = typename T::value_type;
        std::size_t start = in.pos;
        X value;
        if (read(in, value)) out = std::move(value);
        else in.pos = start;
        return true;
    } else if constexpr (k == kind::choice) {
        std::size_t start = in.pos;
        template for (constexpr info a : std::define_static_array(args(^^T))) {
            using A = typename[:a:];
            A value;
            if (read(in, value)) {
                out.template emplace<A>(std::move(value));
                return true;
            }
            in.pos = start;
        }
        return false;
    } else if constexpr (k == kind::token) {
        constexpr const char* text = texts<lit>(^^T)[0];
        return in.literal(text);
    } else {
        template for (constexpr info m : std::define_static_array(fields(^^T))) {
            template for (constexpr const char* p : std::define_static_array(texts<prefix>(m)))
                if (!in.literal(p)) return false;
            if (!read(in, out.[:m:])) return false;
            template for (constexpr const char* s : std::define_static_array(texts<suffix>(m)))
                if (!in.literal(s)) return false;
        }
        return true;
    }
}

template <class T>
void write(std::vector<std::string>& out, const T& value) {
    constexpr kind k = kind_of(^^T);
    if constexpr (k == kind::integer) {
        out.push_back(std::format("{}", value));
    } else if constexpr (k == kind::identifier) {
        out.push_back(value);
    } else if constexpr (k == kind::box) {
        write(out, *value);
    } else if constexpr (k == kind::many) {
        for (const auto& v : value) write(out, v);
    } else if constexpr (k == kind::maybe) {
        if (value) write(out, *value);
    } else if constexpr (k == kind::choice) {
        std::visit([&](const auto& v) { write(out, v); }, value);
    } else if constexpr (k == kind::token) {
        constexpr const char* text = texts<lit>(^^T)[0];
        out.push_back(text);
    } else {
        template for (constexpr info m : std::define_static_array(fields(^^T))) {
            template for (constexpr const char* p : std::define_static_array(texts<prefix>(m))) out.push_back(std::string(p));
            write(out, value.[:m:]);
            template for (constexpr const char* s : std::define_static_array(texts<suffix>(m))) out.push_back(std::string(s));
        }
    }
}

}  // namespace detail

template <class T>
consteval std::string_view grammar() {
    return detail::grammar_text(^^T);
}

template <class T>
std::optional<T> parse(std::string_view text) {
    detail::cursor in{text};
    T out{};
    if (!detail::read(in, out)) return std::nullopt;
    in.skip_space();
    if (in.pos != text.size()) return std::nullopt;
    return out;
}

template <class T>
std::string to_source(const T& value) {
    std::vector<std::string> tokens;
    detail::write(tokens, value);
    std::string out;
    for (const std::string& t : tokens) out += (out.empty() ? "" : " ") + t;
    return out;
}

}  // namespace peg
