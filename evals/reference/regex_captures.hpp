#pragma once

#include <algorithm>
#include <array>
#include <charconv>
#include <cstddef>
#include <meta>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <vector>

namespace rx {

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

namespace detail {

// backtracking program; jump targets are relative to the instruction
enum op : int { lit, any, cls, split, jmp, save, done };

struct inst {
    int code;
    int x;
    int y;
    char c;
};

struct range {
    char lo;
    char hi;
};

struct group {
    std::string name;
    bool is_int;
    bool optional;
};

struct program {
    std::string error;
    std::vector<inst> code;
    std::vector<range> ranges;
    std::vector<group> groups;
};

using frag = std::vector<inst>;

constexpr bool is_word(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}
constexpr bool is_meta(char c) { return std::string_view("\\.[]()*+?|-").contains(c); }

class parser {
public:
    consteval explicit parser(std::string_view s) : src(s) {}

    consteval program run() {
        frag f = alternation();
        if (ok() && pos < src.size()) fail("unmatched ')'");
        f.push_back({done, 0, 0, 0});
        out.code = f;
        return out;
    }

private:
    std::string_view src;
    std::size_t pos = 0;
    program out;

    consteval bool ok() const { return out.error.empty(); }
    consteval void fail(std::string msg) {
        if (ok()) out.error = msg;
    }
    consteval bool at(char c) const { return pos < src.size() && src[pos] == c; }
    consteval void mark_optional(std::size_t from) {
        for (std::size_t g = from; g < out.groups.size(); ++g) out.groups[g].optional = true;
    }

    // a | b | c: try each alternative in order
    consteval frag alternation() {
        std::size_t first_group = out.groups.size();
        std::vector<frag> alts{sequence()};
        while (ok() && at('|')) {
            ++pos;
            alts.push_back(sequence());
        }
        if (alts.size() > 1) mark_optional(first_group);
        frag rest = alts.back();
        for (std::size_t i = alts.size() - 1; i-- > 0;) {
            const frag& a = alts[i];
            frag f{{split, 1, int(a.size()) + 2, 0}};
            f.insert(f.end(), a.begin(), a.end());
            f.push_back({jmp, int(rest.size()) + 1, 0, 0});
            f.insert(f.end(), rest.begin(), rest.end());
            rest = f;
        }
        return rest;
    }

    consteval frag sequence() {
        frag f;
        while (ok() && pos < src.size() && !at('|') && !at(')')) {
            if (at('*') || at('+') || at('?')) {
                fail("nothing to repeat");
                break;
            }
            std::size_t first_group = out.groups.size();
            frag a = atom();
            if (!ok()) break;
            if (at('*') || at('+') || at('?')) {
                char q = src[pos++];
                int n = int(a.size());
                if (q == '*') {
                    a.insert(a.begin(), {split, 1, n + 2, 0});
                    a.push_back({jmp, -(n + 1), 0, 0});
                } else if (q == '+') {
                    a.push_back({split, -n, 1, 0});
                } else {
                    a.insert(a.begin(), {split, 1, n + 1, 0});
                }
                if (q != '+') mark_optional(first_group);
            }
            f.insert(f.end(), a.begin(), a.end());
        }
        return f;
    }

    consteval void add_escape_class(char e) {
        if (e == 'd') out.ranges.push_back({'0', '9'});
        if (e == 'w') {
            out.ranges.push_back({'a', 'z'});
            out.ranges.push_back({'A', 'Z'});
            out.ranges.push_back({'0', '9'});
            out.ranges.push_back({'_', '_'});
        }
        if (e == 's') {
            out.ranges.push_back({' ', ' '});
            out.ranges.push_back({'\t', '\t'});
        }
    }

    // reads the character after a backslash; returns false on error
    consteval bool escape(char& e) {
        if (pos + 1 >= src.size()) {
            fail("trailing backslash");
            return false;
        }
        e = src[pos + 1];
        pos += 2;
        if (e == 'd' || e == 'w' || e == 's' || is_meta(e)) return true;
        fail(std::string("unknown escape '\\") + e + "'");
        return false;
    }

    consteval frag atom() {
        char c = src[pos];
        if (c == '(') return group_atom();
        if (c == '[') return class_atom();
        if (c == '.') {
            ++pos;
            return {{any, 0, 0, 0}};
        }
        if (c == '\\') {
            char e;
            if (!escape(e)) return {};
            if (is_meta(e)) return {{lit, 0, 0, e}};
            int lo = int(out.ranges.size());
            add_escape_class(e);
            return {{cls, lo, int(out.ranges.size()), 0}};
        }
        ++pos;
        return {{lit, 0, 0, c}};
    }

    consteval frag class_atom() {
        ++pos;
        int lo = int(out.ranges.size());
        while (true) {
            if (pos >= src.size()) {
                fail("unclosed class");
                return {};
            }
            char c = src[pos];
            if (c == ']') {
                ++pos;
                break;
            }
            if (c == '\\') {
                char e;
                if (!escape(e)) return {};
                if (is_meta(e)) out.ranges.push_back({e, e});
                else add_escape_class(e);
                continue;
            }
            ++pos;
            if (at('-') && pos + 1 < src.size() && src[pos + 1] != ']' && src[pos + 1] != '\\') {
                out.ranges.push_back({c, src[pos + 1]});
                pos += 2;
            } else {
                out.ranges.push_back({c, c});
            }
        }
        return {{cls, lo, int(out.ranges.size()), 0}};
    }

    consteval frag group_atom() {
        std::string_view rest = src.substr(pos);
        if (rest.starts_with("(?:")) {
            pos += 3;
            frag body = alternation();
            if (!ok()) return {};
            if (!at(')')) {
                fail("unclosed group");
                return {};
            }
            ++pos;
            return body;
        }
        if (!rest.starts_with("(?<")) {
            fail("bad group");
            return {};
        }
        pos += 3;
        std::size_t start = pos;
        while (pos < src.size() && is_word(src[pos])) ++pos;
        std::string name(src.substr(start, pos - start));
        std::string type;
        if (at(':')) {
            start = ++pos;
            while (pos < src.size() && ((src[pos] >= 'a' && src[pos] <= 'z') || (src[pos] >= 'A' && src[pos] <= 'Z'))) ++pos;
            type = src.substr(start, pos - start);
            if (type.empty()) {
                fail("bad group");
                return {};
            }
        }
        if (name.empty() || (name[0] >= '0' && name[0] <= '9') || !at('>')) {
            fail("bad group");
            return {};
        }
        ++pos;
        if (!type.empty() && type != "int") {
            fail("unknown type '" + type + "'");
            return {};
        }
        for (const group& g : out.groups)
            if (g.name == name) {
                fail("duplicate group '" + name + "'");
                return {};
            }
        int k = int(out.groups.size());
        out.groups.push_back({name, type == "int", false});
        frag body = alternation();
        if (!ok()) return {};
        if (!at(')')) {
            fail("unclosed group");
            return {};
        }
        ++pos;
        frag f{{save, 2 * k, 0, 0}};
        f.insert(f.end(), body.begin(), body.end());
        f.push_back({save, 2 * k + 1, 0, 0});
        return f;
    }
};

consteval program parse(std::string_view pattern) { return parser(pattern).run(); }

// recursive backtracking; caps holds start and end of each group, -1 when unset
inline bool run(const inst* prog, const range* ranges, std::string_view text, int pc, std::size_t sp, std::ptrdiff_t* caps) {
    while (true) {
        const inst& in = prog[pc];
        switch (in.code) {
            case lit:
                if (sp >= text.size() || text[sp] != in.c) return false;
                ++sp, ++pc;
                break;
            case any:
                if (sp >= text.size()) return false;
                ++sp, ++pc;
                break;
            case cls: {
                if (sp >= text.size()) return false;
                char c = text[sp];
                if (!std::any_of(ranges + in.x, ranges + in.y, [c](range r) { return c >= r.lo && c <= r.hi; })) return false;
                ++sp, ++pc;
                break;
            }
            case split:
                if (run(prog, ranges, text, pc + in.x, sp, caps)) return true;
                pc += in.y;
                break;
            case jmp:
                pc += in.x;
                break;
            case save: {
                std::ptrdiff_t old = caps[in.x];
                caps[in.x] = std::ptrdiff_t(sp);
                if (run(prog, ranges, text, pc + 1, sp, caps)) return true;
                caps[in.x] = old;
                return false;
            }
            default:
                return sp == text.size();
        }
    }
}

template <fixed_string P>
struct regex {
    static constexpr std::string_view error = std::define_static_string(parse(P.view()).error);
    static constexpr auto code = std::define_static_array(parse(P.view()).code);
    static constexpr auto ranges = std::define_static_array(parse(P.view()).ranges);
    static constexpr std::size_t groups = parse(P.view()).groups.size();

    struct captures;
    consteval {
        program p = parse(P.view());
        if (p.error.empty()) {
            std::vector<std::meta::info> specs;
            for (const group& g : p.groups) {
                std::meta::info type = g.is_int ? ^^int : ^^std::string_view;
                if (g.optional) type = std::meta::substitute(^^std::optional, {type});
                specs.push_back(std::meta::data_member_spec(type, {.name = g.name}));
            }
            std::meta::define_aggregate(^^captures, specs);
        }
    }
};

}  // namespace detail

// "ok", or the first error in the pattern
consteval std::string_view check(std::string_view pattern) {
    std::string error = detail::parse(pattern).error;
    return error.empty() ? "ok" : std::define_static_string(error);
}

template <fixed_string P>
std::optional<typename detail::regex<P>::captures> match(std::string_view text) {
    using R = detail::regex<P>;
    static_assert(R::error.empty(), "invalid pattern");
    using C = typename R::captures;

    std::array<std::ptrdiff_t, 2 * R::groups> caps;
    caps.fill(-1);
    if (!detail::run(R::code.data(), R::ranges.data(), text, 0, 0, caps.data())) return std::nullopt;

    C out{};
    std::size_t k = 0;
    template for (constexpr auto m :
                  std::define_static_array(std::meta::nonstatic_data_members_of(^^C, std::meta::access_context::unprivileged()))) {
        using M = typename[:std::meta::type_of(m):];
        std::ptrdiff_t b = caps[2 * k], e = caps[2 * k + 1];
        ++k;
        if (b >= 0) {
            std::string_view s = text.substr(std::size_t(b), std::size_t(e - b));
            if constexpr (std::is_same_v<M, int> || std::is_same_v<M, std::optional<int>>) {
                int v = 0;
                auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), v);
                if (ec != std::errc{} || end != s.data() + s.size()) return std::nullopt;
                out.[:m:] = v;
            } else {
                out.[:m:] = s;
            }
        }
    }
    return out;
}

}  // namespace rx
