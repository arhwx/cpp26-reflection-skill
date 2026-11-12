#pragma once

#include <charconv>
#include <cstddef>
#include <format>
#include <meta>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace ini {
using std::meta::info;

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

constexpr std::string_view trim(std::string_view s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.remove_prefix(1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.remove_suffix(1);
    return s;
}

constexpr bool is_digit(char c) { return c >= '0' && c <= '9'; }
constexpr bool is_alpha(char c) { return c == '_' || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }

constexpr bool is_identifier(std::string_view s) {
    if (s.empty() || !is_alpha(s[0])) return false;
    for (char c : s)
        if (!is_alpha(c) && !is_digit(c)) return false;
    return true;
}

constexpr std::size_t digits(std::string_view s, std::size_t i) {
    std::size_t n = 0;
    while (i + n < s.size() && is_digit(s[i + n])) ++n;
    return n;
}

// optional '-', digits, and when `fraction` is 2 a required '.' and digits; 1 makes the fraction optional
constexpr bool is_number(std::string_view s, int fraction) {
    std::size_t i = s.starts_with('-') ? 1 : 0;
    std::size_t n = digits(s, i);
    if (n == 0) return false;
    i += n;
    if (i == s.size()) return fraction != 2;
    if (fraction == 0 || s[i] != '.') return false;
    n = digits(s, i + 1);
    return n > 0 && i + 1 + n == s.size();
}

constexpr bool is_quoted(std::string_view s) { return s.size() >= 2 && s.front() == '"' && s.back() == '"'; }

// one line of INI text
struct line {
    enum kind_t { blank, header, pair, bad } kind;
    std::string_view name;
    std::string_view value;
};

constexpr line classify(std::string_view raw) {
    std::string_view s = trim(raw);
    if (s.empty() || s.front() == ';' || s.front() == '#') return {line::blank, {}, {}};
    if (s.front() == '[' && s.back() == ']') return {line::header, trim(s.substr(1, s.size() - 2)), {}};
    std::size_t eq = s.find('=');
    if (eq == std::string_view::npos) return {line::bad, {}, {}};
    return {line::pair, trim(s.substr(0, eq)), trim(s.substr(eq + 1))};
}

// calls f(number, line) for every line, numbered from 1
constexpr void for_lines(std::string_view text, auto f) {
    std::size_t number = 1;
    while (true) {
        std::size_t end = text.find('\n');
        f(number, classify(text.substr(0, end)));
        if (end == std::string_view::npos) break;
        text.remove_prefix(end + 1);
        ++number;
    }
}

consteval info type_for(std::string_view value) {
    if (value == "true" || value == "false") return ^^bool;
    if (is_number(value, 0)) return ^^int;
    if (is_number(value, 2)) return ^^double;
    return ^^std::string;
}

struct key_def {
    std::string_view name;
    std::string_view value;
};
struct section_def {
    std::string_view name;
    std::vector<key_def> keys;
};
struct model {
    std::vector<key_def> top;
    std::vector<section_def> sections;
    std::string error;
};

consteval std::string number(std::size_t n) { return (n > 9 ? number(n / 10) : "") + char('0' + n % 10); }

consteval model read_sample(std::string_view text) {
    model m;
    bool in_section = false;
    for_lines(text, [&](std::size_t n, line l) {
        if (!m.error.empty() || l.kind == line::blank) return;
        std::string at = "line " + number(n) + ": ";
        if (l.kind == line::bad) {
            m.error = at + "expected 'key = value'";
        } else if (!is_identifier(l.name)) {
            m.error = at + "'" + std::string(l.name) + "' is not an identifier";
        } else if (l.kind == line::header) {
            for (const section_def& s : m.sections)
                if (s.name == l.name) m.error = at + "section '" + std::string(l.name) + "' appears twice";
            for (const key_def& k : m.top)
                if (m.error.empty() && k.name == l.name) m.error = at + "section '" + std::string(l.name) + "' has the name of a key";
            in_section = true;
            m.sections.push_back({l.name, {}});
        } else {
            std::vector<key_def>& keys = in_section ? m.sections.back().keys : m.top;
            for (const key_def& k : keys)
                if (k.name == l.name) m.error = at + "key '" + std::string(l.name) + "' appears twice";
            keys.push_back({l.name, l.value});
        }
    });
    return m;
}

consteval std::string_view kind_name(info t) {
    return t == ^^bool ? "bool" : t == ^^int ? "int" : t == ^^double ? "double" : "string";
}

consteval std::vector<info> fields(info type) {
    return std::meta::nonstatic_data_members_of(type, std::meta::access_context::unprivileged());
}

consteval bool is_section(info member) {
    info t = std::meta::type_of(member);
    return std::meta::is_class_type(t) && !std::meta::is_same_type(t, ^^std::string);
}

consteval std::vector<info> scalars(info type) {
    std::vector<info> out;
    for (info m : fields(type))
        if (!is_section(m)) out.push_back(m);
    return out;
}

consteval std::vector<info> sections(info type) {
    std::vector<info> out;
    for (info m : fields(type))
        if (is_section(m)) out.push_back(m);
    return out;
}

inline bool convert(bool& out, std::string_view v) {
    if (v != "true" && v != "false") return false;
    out = v == "true";
    return true;
}
inline bool convert(int& out, std::string_view v) {
    if (!is_number(v, 0)) return false;
    auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
    return ec == std::errc{};
}
inline bool convert(double& out, std::string_view v) {
    if (!is_number(v, 1)) return false;
    auto [end, ec] = std::from_chars(v.data(), v.data() + v.size(), out);
    return ec == std::errc{};
}
inline bool convert(std::string& out, std::string_view v) {
    out = is_quoted(v) ? v.substr(1, v.size() - 2) : v;
    return true;
}

inline std::string show(bool v) { return v ? "true" : "false"; }
inline std::string show(int v) { return std::format("{}", v); }
inline std::string show(double v) { return std::format("{}", v); }
inline std::string show(const std::string& v) { return "\"" + v + "\""; }

// sets the key of obj; false if obj has no such key
template <class Obj>
bool set_key(Obj& obj, std::string_view prefix, line l, std::size_t n, std::vector<std::string>& errors) {
    template for (constexpr info m : std::define_static_array(scalars(^^Obj))) {
        if (std::meta::identifier_of(m) == l.name) {
            constexpr std::string_view kind = kind_name(std::meta::type_of(m));
            if (!convert(obj.[:m:], l.value))
                errors.push_back(std::format("line {}: {}{} expects {}, got '{}'", n, prefix, l.name, kind, l.value));
            return true;
        }
    }
    return false;
}

template <class Obj>
void write_keys(std::string& out, const Obj& obj) {
    template for (constexpr info m : std::define_static_array(scalars(^^Obj)))
        out += std::format("{} = {}\n", std::meta::identifier_of(m), show(obj.[:m:]));
}

}  // namespace detail

consteval std::string_view problem(std::string_view sample) {
    detail::model m = detail::read_sample(sample);
    return m.error.empty() ? std::string_view("ok") : std::define_static_string(m.error);
}

template <fixed_string Sample>
struct schema {
    static_assert(problem(Sample.view()) == "ok", "the sample INI text is not valid, see ini::problem");

    template <std::size_t I>
    struct section;
    struct type;

    consteval {
        detail::model m = detail::read_sample(Sample.view());
        if (m.error.empty()) {
            std::vector<info> top;
            for (const detail::key_def& k : m.top)
                top.push_back(std::meta::data_member_spec(detail::type_for(k.value), {.name = k.name}));
            for (std::size_t i = 0; i < m.sections.size(); ++i) {
                std::vector<info> keys;
                for (const detail::key_def& k : m.sections[i].keys)
                    keys.push_back(std::meta::data_member_spec(detail::type_for(k.value), {.name = k.name}));
                info s = std::meta::define_aggregate(std::meta::substitute(^^section, {std::meta::reflect_constant(i)}), keys);
                top.push_back(std::meta::data_member_spec(s, {.name = m.sections[i].name}));
            }
            std::meta::define_aggregate(^^type, top);
        }
    }

    static consteval std::string_view describe() {
        std::string out;
        for (info m : detail::fields(^^type)) {
            if (!detail::is_section(m)) {
                out += std::string(std::meta::identifier_of(m)) + ": " + std::string(detail::kind_name(std::meta::type_of(m))) + "\n";
                continue;
            }
            for (info k : detail::fields(std::meta::type_of(m)))
                out += std::string(std::meta::identifier_of(m)) + "." + std::string(std::meta::identifier_of(k)) + ": " +
                       std::string(detail::kind_name(std::meta::type_of(k))) + "\n";
        }
        if (!out.empty()) out.pop_back();
        return std::define_static_string(out);
    }

    static type parse(std::string_view text, std::vector<std::string>& errors) {
        type out = defaults();
        apply(out, text, errors);
        return out;
    }

    static type defaults() {
        type out{};
        std::vector<std::string> ignored;
        apply(out, Sample.view(), ignored);
        return out;
    }

    static std::string write(const type& value) {
        std::string out;
        detail::write_keys(out, value);
        template for (constexpr info s : std::define_static_array(detail::sections(^^type))) {
            if (!out.empty()) out += "\n";
            out += std::format("[{}]\n", std::meta::identifier_of(s));
            detail::write_keys(out, value.[:s:]);
        }
        return out;
    }

private:
    static constexpr int top_level = -1, unknown = -2;

    static void apply(type& out, std::string_view text, std::vector<std::string>& errors) {
        int current = top_level;
        detail::for_lines(text, [&](std::size_t n, detail::line l) {
            if (l.kind == detail::line::blank) return;
            if (l.kind == detail::line::header) {
                current = unknown;
                int i = 0;
                template for (constexpr info s : std::define_static_array(detail::sections(^^type))) {
                    if (std::meta::identifier_of(s) == l.name) current = i;
                    ++i;
                }
                if (current == unknown) errors.push_back(std::format("line {}: unknown section '{}'", n, l.name));
                return;
            }
            if (current == unknown) return;
            if (l.kind == detail::line::bad) {
                errors.push_back(std::format("line {}: expected 'key = value'", n));
                return;
            }
            bool known = false;
            std::string prefix;
            if (current == top_level) {
                known = detail::set_key(out, "", l, n, errors);
            } else {
                int i = 0;
                template for (constexpr info s : std::define_static_array(detail::sections(^^type))) {
                    if (i++ == current) {
                        prefix = std::format("{}.", std::meta::identifier_of(s));
                        known = detail::set_key(out.[:s:], prefix, l, n, errors);
                    }
                }
            }
            if (!known) errors.push_back(std::format("line {}: unknown key '{}{}'", n, prefix, l.name));
        });
    }
};

}  // namespace ini
