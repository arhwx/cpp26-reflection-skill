#pragma once

#include <cstddef>
#include <meta>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// holds a string literal so it can be a template argument
template <std::size_t N>
struct fixed_string {
    char data[N]{};

    constexpr fixed_string(const char (&s)[N]) {
        for (std::size_t i = 0; i < N; ++i)
            data[i] = s[i];
    }

    constexpr std::string_view view() const { return {data, N - 1}; }
};

namespace detail {

// a piece of a parsed format: literal text, or the index of a field
struct segment {
    bool is_field;
    const char* text;
    std::size_t field;
};

// placeholders can only name public non-static data members
consteval std::vector<std::meta::info> fields(std::meta::info type) {
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

// std::string is an alias, so compare with is_same_type rather than ==
consteval bool printable(std::meta::info type) {
    type = std::meta::remove_cv(type);
    return std::meta::is_integral_type(type) || std::meta::is_same_type(type, ^^std::string) ||
           std::meta::is_same_type(type, ^^std::string_view) || std::meta::is_same_type(type, ^^const char*);
}

// splits fmt into segments, returns false when fmt is not valid for type
consteval bool parse(std::string_view fmt, std::meta::info type, std::vector<segment>& out) {
    std::vector<std::meta::info> members = fields(type);
    std::string text;
    auto flush = [&] {
        if (!text.empty())
            out.push_back({false, std::define_static_string(text), 0});
        text.clear();
    };
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        char c = fmt[i];
        if ((c == '{' || c == '}') && i + 1 < fmt.size() && fmt[i + 1] == c) {
            text += c;
            ++i;
        } else if (c == '{') {
            std::string name;
            for (++i; i < fmt.size() && fmt[i] != '}'; ++i)
                name += fmt[i];
            if (i == fmt.size())
                return false;
            std::size_t found = members.size();
            for (std::size_t m = 0; m < members.size(); ++m)
                if (std::meta::has_identifier(members[m]) && std::meta::identifier_of(members[m]) == name)
                    found = m;
            if (found == members.size() || !printable(std::meta::type_of(members[found])))
                return false;
            flush();
            out.push_back({true, nullptr, found});
        } else if (c == '}') {
            return false;
        } else {
            text += c;
        }
    }
    flush();
    return true;
}

// used in the constraint, so it reports errors instead of throwing
consteval bool valid(std::string_view fmt, std::meta::info type) {
    std::vector<segment> out;
    return parse(fmt, type, out);
}

consteval std::vector<segment> segments(std::string_view fmt, std::meta::info type) {
    std::vector<segment> out;
    parse(fmt, type, out);
    return out;
}

template <class V>
void append(std::string& out, const V& v) {
    if constexpr (std::is_integral_v<V>)
        out += std::to_string(v);
    else
        out += v;
}

}  // namespace detail

template <fixed_string Fmt, class T>
    requires(detail::valid(Fmt.view(), ^^T))
std::string format_fields(const T& value) {
    std::string out;
    template for (constexpr detail::segment s : std::define_static_array(detail::segments(Fmt.view(), ^^T))) {
        if constexpr (s.is_field) {
            constexpr std::meta::info m = detail::fields(^^T)[s.field];
            detail::append(out, value.[:m:]);
        } else {
            out += s.text;
        }
    }
    return out;
}
