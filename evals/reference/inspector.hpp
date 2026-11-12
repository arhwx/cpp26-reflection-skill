#pragma once

#include <charconv>
#include <format>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>

namespace inspect {

struct range {
    double lo;
    double hi;
    constexpr range(double lo, double hi) : lo(lo), hi(hi) {}
};

struct label {
    const char* text;
    consteval label(std::string_view text) : text(std::define_static_string(text)) {}
};

struct readonly_t {};
inline constexpr readonly_t readonly{};

namespace detail {

constexpr auto ctx = std::meta::access_context::unprivileged();

// the annotation of type A on member m, or a null reflection
consteval std::meta::info find(std::meta::info m, std::meta::info type) {
    for (std::meta::info a : std::meta::annotations_of(m))
        // class-type annotations are const-qualified
        if (std::meta::remove_const(std::meta::type_of(a)) == type)
            return a;
    return {};
}

consteval std::string_view label_of(std::meta::info m) {
    std::meta::info a = find(m, ^^label);
    return a == std::meta::info{} ? std::meta::identifier_of(m) : std::meta::extract<label>(a).text;
}

template <class M>
constexpr bool is_group = std::is_class_v<M> && !std::is_same_v<M, std::string>;

template <class E>
std::string choices(E value) {
    std::string out;
    template for (constexpr std::meta::info e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (!out.empty())
            out += ' ';
        bool on = value == [:e:];
        out += on ? '[' : '(';
        out += std::meta::identifier_of(e);
        out += on ? ']' : ')';
    }
    return out;
}

template <class E>
std::string names() {
    std::string out;
    template for (constexpr std::meta::info e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (!out.empty())
            out += ", ";
        out += std::meta::identifier_of(e);
    }
    return out;
}

template <class T>
void render_fields(std::string& out, const T& obj, int depth, bool locked) {
    std::string indent(2 * depth, ' ');
    template for (constexpr std::meta::info m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
        using M = typename [:std::meta::type_of(m):];
        constexpr std::string_view name = label_of(m);
        constexpr bool ro = find(m, ^^readonly_t) != std::meta::info{};
        const M& value = obj.[:m:];
        if constexpr (is_group<M>) {
            out += std::format("{}{}:\n", indent, name);
            render_fields(out, value, depth + 1, locked || ro);
        } else {
            out += std::format("{}{} = ", indent, name);
            if constexpr (std::is_same_v<M, bool>) {
                out += value ? "[x]" : "[ ]";
            } else if constexpr (std::is_enum_v<M>) {
                out += choices(value);
            } else if constexpr (std::is_same_v<M, std::string>) {
                out += std::format("\"{}\"", value);
            } else {
                out += std::format("{}", value);
                constexpr std::meta::info a = find(m, ^^range);
                if constexpr (a != std::meta::info{}) {
                    constexpr range r = std::meta::extract<range>(a);
                    out += std::format(" ({}..{})", r.lo, r.hi);
                    if (value < r.lo || value > r.hi)
                        out += " out of range";
                }
            }
            if (locked || ro)
                out += " (read-only)";
            out += '\n';
        }
    }
}

// parse text into a leaf member m; an empty result means success
template <std::meta::info m, class M>
std::string parse(M& target, std::string_view text) {
    if constexpr (std::is_same_v<M, bool>) {
        if (text == "true" || text == "false") {
            target = text == "true";
            return "";
        }
        return std::format("\"{}\" is not true or false", text);
    } else if constexpr (std::is_enum_v<M>) {
        template for (constexpr std::meta::info e : std::define_static_array(std::meta::enumerators_of(^^M))) {
            if (text == std::meta::identifier_of(e)) {
                target = [:e:];
                return "";
            }
        }
        return std::format("\"{}\" is not one of {}", text, names<M>());
    } else if constexpr (std::is_same_v<M, std::string>) {
        target = std::string(text);
        return "";
    } else {
        M value{};
        auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
        if (ec != std::errc{} || end != text.data() + text.size() || text.empty())
            return std::format("\"{}\" is not {}", text, std::is_integral_v<M> ? "a whole number" : "a number");
        constexpr std::meta::info a = find(m, ^^range);
        if constexpr (a != std::meta::info{}) {
            constexpr range r = std::meta::extract<range>(a);
            if (value < r.lo || value > r.hi)
                return std::format("{} is outside {}..{}", value, r.lo, r.hi);
        }
        target = value;
        return "";
    }
}

template <class T>
std::string set_in(T& obj, std::string_view path, std::string_view text, bool locked) {
    std::size_t dot = path.find('.');
    std::string_view head = path.substr(0, dot);
    std::string_view rest = dot == std::string_view::npos ? std::string_view{} : path.substr(dot + 1);
    bool deeper = dot != std::string_view::npos;
    template for (constexpr std::meta::info m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
        using M = typename [:std::meta::type_of(m):];
        constexpr bool ro = find(m, ^^readonly_t) != std::meta::info{};
        if (head == std::meta::identifier_of(m)) {
            if constexpr (is_group<M>) {
                if (!deeper)
                    return "is a group";
                return set_in(obj.[:m:], rest, text, locked || ro);
            } else {
                if (deeper)
                    return "no such field";
                if (locked || ro)
                    return "read-only";
                return parse<m>(obj.[:m:], text);
            }
        }
    }
    return "no such field";
}

}  // namespace detail

template <class T>
std::string render(const T& obj) {
    std::string out = std::format("{}\n", std::meta::identifier_of(^^T));
    detail::render_fields(out, obj, 1, false);
    return out;
}

template <class T>
std::string set(T& obj, std::string_view path, std::string_view text) {
    std::string problem = detail::set_in(obj, path, text, false);
    return problem.empty() ? std::string("ok") : std::format("{}: {}", path, problem);
}

}  // namespace inspect
