#pragma once

#include <cstddef>
#include <format>
#include <meta>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace inv {
using std::meta::info;

// the class annotation
struct invariant {
    const char* text;
    consteval invariant(std::string_view t) : text(std::define_static_string(t)) {}
};

class violation : public std::logic_error {
public:
    using std::logic_error::logic_error;
};

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) {
        for (std::size_t i = 0; i < N; ++i)
            data[i] = s[i];
    }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

namespace detail {

// expression nodes; a parsed invariant is a type built from these
template <double V>
struct num {
    static constexpr double eval(const auto&) { return V; }
};
template <info M>
struct ref {
    static constexpr double eval(const auto& obj) { return static_cast<double>(obj.[:M:]); }
};
template <char Op, class X>
struct unary {
    static constexpr double eval(const auto& obj) {
        double x = X::eval(obj);
        return Op == '!' ? (x == 0 ? 1.0 : 0.0) : -x;
    }
};
template <char Op, class L, class R>
struct binary {
    static constexpr double eval(const auto& obj) {
        double a = L::eval(obj), b = R::eval(obj);
        switch (Op) {
        case '|': return a != 0 || b != 0;
        case '&': return a != 0 && b != 0;
        case 'e': return a == b;
        case 'n': return a != b;
        case '<': return a < b;
        case 'l': return a <= b;
        case '>': return a > b;
        case 'g': return a >= b;
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        default: return a / b;
        }
    }
};

consteval std::vector<info> fields(info type) {
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

consteval std::vector<const char*> texts(info type) {
    std::vector<const char*> out;
    for (info a : std::meta::annotations_of(type))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^invariant)
            out.push_back(std::meta::extract<invariant>(a).text);
    return out;
}

constexpr bool is_digit(char c) { return c >= '0' && c <= '9'; }
constexpr bool is_alpha(char c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

// recursive descent; on the first problem `error` is set and every level returns at once
struct parser {
    info type;
    std::string_view text;
    std::size_t pos = 0;
    std::string error{};
    std::vector<info> names{};
    info root{};

    consteval void skip() {
        while (pos < text.size() && text[pos] == ' ')
            ++pos;
    }
    consteval bool eat(std::string_view token) {
        skip();
        if (!text.substr(pos).starts_with(token))
            return false;
        pos += token.size();
        return true;
    }
    consteval info fail() {
        if (error.empty())
            error = "cannot parse";
        return {};
    }
    consteval info node(char op, info l, info r) {
        return std::meta::substitute(^^binary, {std::meta::reflect_constant(op), l, r});
    }

    consteval info disjunction() {
        info l = conjunction();
        while (error.empty() && eat("||")) {
            info r = conjunction();
            if (!error.empty()) return {};
            l = node('|', l, r);
        }
        return l;
    }
    consteval info conjunction() {
        info l = comparison();
        while (error.empty() && eat("&&")) {
            info r = comparison();
            if (!error.empty()) return {};
            l = node('&', l, r);
        }
        return l;
    }
    consteval info comparison() {
        info l = sum();
        if (!error.empty()) return {};
        constexpr std::pair<const char*, char> ops[] = {{"==", 'e'}, {"!=", 'n'}, {"<=", 'l'}, {">=", 'g'}, {"<", '<'}, {">", '>'}};
        for (auto [token, op] : ops)
            if (eat(token)) {
                info r = sum();
                if (!error.empty()) return {};
                return node(op, l, r);
            }
        return l;
    }
    consteval info sum() {
        info l = product();
        while (error.empty()) {
            char op = eat("+") ? '+' : eat("-") ? '-' : 0;
            if (!op) break;
            info r = product();
            if (!error.empty()) return {};
            l = node(op, l, r);
        }
        return l;
    }
    consteval info product() {
        info l = factor();
        while (error.empty()) {
            char op = eat("*") ? '*' : eat("/") ? '/' : 0;
            if (!op) break;
            info r = factor();
            if (!error.empty()) return {};
            l = node(op, l, r);
        }
        return l;
    }
    consteval info factor() {
        if (eat("!") || eat("-")) {
            char op = text[pos - 1];
            info x = factor();
            if (!error.empty()) return {};
            return std::meta::substitute(^^unary, {std::meta::reflect_constant(op), x});
        }
        if (eat("(")) {
            info e = disjunction();
            if (!error.empty()) return {};
            if (!eat(")")) return fail();
            return e;
        }
        skip();
        if (pos < text.size() && is_digit(text[pos])) {
            double v = 0;
            while (pos < text.size() && is_digit(text[pos]))
                v = v * 10 + (text[pos++] - '0');
            if (pos < text.size() && text[pos] == '.') {
                ++pos;
                if (pos == text.size() || !is_digit(text[pos])) return fail();
                double scale = 1;
                while (pos < text.size() && is_digit(text[pos]))
                    v = v * 10 + (text[pos++] - '0'), scale *= 10;
                v /= scale;
            }
            return std::meta::substitute(^^num, {std::meta::reflect_constant(v)});
        }
        if (pos < text.size() && is_alpha(text[pos])) {
            std::size_t start = pos;
            while (pos < text.size() && (is_alpha(text[pos]) || is_digit(text[pos])))
                ++pos;
            std::string_view name = text.substr(start, pos - start);
            for (info m : fields(type))
                if (std::meta::identifier_of(m) == name) {
                    if (std::ranges::find(names, m) == names.end())
                        names.push_back(m);
                    return std::meta::substitute(^^ref, {std::meta::reflect_constant(m)});
                }
            error = "unknown name '" + std::string(name) + "' in";
            return {};
        }
        return fail();
    }

    consteval info parse() {
        info e = disjunction();
        skip();
        if (error.empty() && pos != text.size())
            return fail();
        return e;
    }
};

consteval parser compile(info type, const char* text) {
    parser p{type, text};
    p.root = p.parse();
    return p;
}

consteval std::vector<info> expressions(info type) {
    std::vector<info> out;
    for (const char* t : texts(type))
        out.push_back(compile(type, t).root);
    return out;
}

}  // namespace detail

consteval std::string_view check(info type) {
    for (const char* t : detail::texts(type)) {
        detail::parser p = detail::compile(type, t);
        if (!p.error.empty())
            return std::define_static_string(std::string(std::meta::identifier_of(type)) + ": " + p.error + " '" + t + "'");
    }
    return "ok";
}

namespace detail {

template <class T>
inline constexpr auto texts_of = std::define_static_array(texts(^^T));
template <class T>
inline constexpr auto exprs_of = std::define_static_array(expressions(^^T));
template <class T, std::size_t I>
inline constexpr auto names_of = std::define_static_array(compile(^^T, texts_of<T>[I]).names);

template <class T>
constexpr bool valid = check(^^T) == "ok";

// true for each invariant that holds, in annotation order
template <class T>
constexpr void evaluate(const T& value, auto&& on_each) {
    template for (constexpr std::size_t i : std::define_static_array(std::views::iota(0uz, texts_of<T>.size()))) {
        using expr = typename [:exprs_of<T>[i]:];
        on_each.template operator()<i>(expr::eval(value) != 0);
    }
}

template <class T, std::size_t I>
std::string message(const T& value) {
    std::string out = std::format("{}: '{}' is broken", std::meta::identifier_of(^^T), texts_of<T>[I]);
    std::string list;
    template for (constexpr info m : names_of<T, I>) {
        if (!list.empty()) list += ", ";
        list += std::format("{} = {}", std::meta::identifier_of(m), value.[:m:]);
    }
    if (!list.empty()) out += " (" + list + ")";
    return out;
}

template <class T>
void enforce(const T& value) {
    std::string first;
    evaluate(value, [&]<std::size_t I>(bool ok) {
        if (!ok && first.empty()) first = message<T, I>(value);
    });
    if (!first.empty()) throw violation(first);
}

consteval bool has_field(info type, std::string_view name) {
    for (info m : fields(type))
        if (std::meta::identifier_of(m) == name) return true;
    return false;
}

consteval info field(info type, std::string_view name) {
    for (info m : fields(type))
        if (std::meta::identifier_of(m) == name) return m;
    return {};
}

}  // namespace detail

template <class T>
constexpr bool holds(const T& value) {
    static_assert(detail::valid<T>, "an invariant of T cannot be parsed, see inv::check");
    bool all = true;
    detail::evaluate(value, [&]<std::size_t>(bool ok) { all = all && ok; });
    return all;
}

template <class T>
std::vector<std::string> broken(const T& value) {
    static_assert(detail::valid<T>, "an invariant of T cannot be parsed, see inv::check");
    std::vector<std::string> out;
    detail::evaluate(value, [&]<std::size_t I>(bool ok) {
        if (!ok) out.emplace_back(detail::texts_of<T>[I]);
    });
    return out;
}

template <class T>
class checked {
    static_assert(detail::valid<T>, "an invariant of T cannot be parsed, see inv::check");

public:
    explicit checked(T value) : value_(std::move(value)) { detail::enforce(value_); }

    const T& get() const { return value_; }
    const T* operator->() const { return &value_; }

    template <fixed_string Name, class V>
        requires(detail::has_field(^^T, Name.view()))
    void set(V&& v) {
        constexpr info m = detail::field(^^T, Name.view());
        T next = value_;
        next.[:m:] = std::forward<V>(v);
        detail::enforce(next);
        value_ = std::move(next);
    }

    template <class F>
    void modify(F f) {
        T next = value_;
        f(next);
        detail::enforce(next);
        value_ = std::move(next);
    }

private:
    T value_;
};

}  // namespace inv
