#pragma once

#include <cstddef>
#include <format>
#include <meta>
#include <string>
#include <string_view>
#include <vector>

namespace fmt {

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

using std::meta::info;

// literal text, or a field with its std::format replacement field such as "{:>4d}"
struct segment {
    bool is_field;
    const char* text;
    std::size_t field;
};

struct field {
    std::string_view name;
    info type;
};

// the presentation type at the end of the spec picks the member type
consteval info type_for(std::string_view spec) {
    char last = spec.empty() ? 's' : spec.back();
    switch (last) {
    case 'd': case 'x': case 'X': case 'o': case 'b':
        return ^^int;
    case 'f': case 'e': case 'g':
        return ^^double;
    case 'c':
        return ^^char;
    default:
        return ^^std::string;
    }
}

consteval bool is_identifier(std::string_view name) {
    auto alpha = [](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; };
    if (name.empty() || !alpha(name[0]))
        return false;
    for (char c : name)
        if (!alpha(c) && !(c >= '0' && c <= '9'))
            return false;
    return true;
}

// splits fmt into segments and fields, returns false when fmt is not valid
consteval bool parse(std::string_view fmt, std::vector<segment>& segments, std::vector<field>& fields) {
    std::string text;
    auto flush = [&] {
        if (!text.empty())
            segments.push_back({false, std::define_static_string(text), 0});
        text.clear();
    };
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        char c = fmt[i];
        if ((c == '{' || c == '}') && i + 1 < fmt.size() && fmt[i + 1] == c) {
            text += c;
            ++i;
        } else if (c == '}') {
            return false;
        } else if (c == '{') {
            std::size_t close = fmt.find('}', i);
            if (close == std::string_view::npos)
                return false;
            std::string_view inside = fmt.substr(i + 1, close - i - 1);
            std::size_t colon = inside.find(':');
            std::string_view name = inside.substr(0, colon);
            std::string_view spec = colon == std::string_view::npos ? "" : inside.substr(colon + 1);
            if (!is_identifier(name))
                return false;
            info type = type_for(spec);
            std::size_t index = 0;
            while (index < fields.size() && fields[index].name != name)
                ++index;
            if (index == fields.size())
                fields.push_back({name, type});
            else if (fields[index].type != type)
                return false;
            flush();
            segments.push_back({true, std::define_static_string("{:" + std::string(spec) + "}"), index});
            i = close;
        } else {
            text += c;
        }
    }
    flush();
    return true;
}

consteval bool valid(std::string_view fmt) {
    std::vector<segment> segments;
    std::vector<field> fields;
    return parse(fmt, segments, fields);
}

consteval std::vector<segment> segments_of(std::string_view fmt) {
    std::vector<segment> segments;
    std::vector<field> fields;
    parse(fmt, segments, fields);
    return segments;
}

template <fixed_string F>
struct holder {
    struct type;
    consteval {
        std::vector<segment> segments;
        std::vector<field> fields;
        parse(F.view(), segments, fields);
        std::vector<info> specs;
        for (const field& f : fields)
            specs.push_back(std::meta::data_member_spec(f.type, {.name = f.name}));
        std::meta::define_aggregate(^^type, specs);
    }
};

}  // namespace detail

template <fixed_string F>
    requires(detail::valid(F.view()))
using args = typename detail::holder<F>::type;

template <fixed_string F>
    requires(detail::valid(F.view()))
std::string format(const args<F>& values) {
    static constexpr auto members = std::define_static_array(
        std::meta::nonstatic_data_members_of(^^args<F>, std::meta::access_context::unchecked()));
    std::string out;
    template for (constexpr detail::segment s : std::define_static_array(detail::segments_of(F.view()))) {
        if constexpr (s.is_field)
            out += std::vformat(s.text, std::make_format_args(values.[:members[s.field]:]));
        else
            out += s.text;
    }
    return out;
}

}  // namespace fmt
