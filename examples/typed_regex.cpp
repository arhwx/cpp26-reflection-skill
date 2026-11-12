// typed_regex: named regex groups become struct members. (?<year:int>\d+) gives an int member named year,
// a group that can be left out gives a std::optional, and a bad pattern does not compile
#include <algorithm>
#include <array>
#include <charconv>
#include <meta>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace rx {
using std::meta::info;

template <std::size_t N>
struct fixed_string {
    char data[N]{};
    consteval fixed_string(const char (&s)[N]) { std::ranges::copy(s, data); }
    constexpr std::string_view view() const { return {data, N - 1}; }
};

// a pattern compiles to a program for a backtracking matcher
enum op { one, any, digit, word, split, jmp, save, done };  // the first four read a character. one: the character x
struct inst { op kind; int x = 0, y = 0; };
struct capture { std::string name; bool is_int, required = true; };
using code = std::vector<inst>;

constexpr bool is_digit(char c) { return c >= '0' && c <= '9'; }
constexpr bool is_word(char c) { return is_digit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }

struct parser {
    std::string_view s;
    std::size_t pos = 0;
    std::string error{};
    std::vector<capture> groups{};

    consteval code fail(std::string what) {  // keeps the first error and stops reading
        if (error.empty()) error = what;
        pos = s.size();
        return {};
    }
    consteval bool next_in(std::string_view set) const { return pos < s.size() && set.contains(s[pos]); }
    consteval bool eat(char c) { return pos < s.size() && s[pos] == c && (++pos, true); }
    consteval void optional_from(std::size_t g) { for (; g < groups.size(); ++g) groups[g].required = false; }

    consteval code all() {
        code out = alternatives();
        if (pos < s.size()) return fail("unmatched ')'");
        out.push_back({done});
        return out;
    }
    consteval code alternatives() {  // a|b tries a first
        std::size_t first = groups.size();
        code a = sequence();
        if (!eat('|')) return a;
        code b = alternatives();
        optional_from(first);
        a.insert(a.begin(), {split, 1, int(a.size()) + 2});
        a.push_back({jmp, int(b.size()) + 1});
        a.append_range(b);
        return a;
    }
    consteval code sequence() {  // atoms, each with an optional * + or ?
        code out;
        while (pos < s.size() && !next_in("|)")) {
            if (next_in("*+?")) return fail("nothing to repeat");
            std::size_t first = groups.size();
            code a = atom();
            int n = int(a.size());
            if (eat('*')) a.insert(a.begin(), {split, 1, n + 2}), a.push_back({jmp, -n - 1}), optional_from(first);
            else if (eat('?')) a.insert(a.begin(), {split, 1, n + 1}), optional_from(first);
            else if (eat('+')) a.push_back({split, -n, 1});
            out.append_range(a);
        }
        return out;
    }
    consteval code atom() {
        char c = s[pos++];
        if (c == '(') return group();
        if (c == '.') return {{any}};
        if (c != '\\') return {{one, c}};
        if (pos == s.size()) return fail("trailing backslash");
        char e = s[pos++];
        if (e == 'd' || e == 'w') return {{e == 'd' ? digit : word}};
        if (!std::string_view("\\.[]()*+?|").contains(e)) return fail(std::string("unknown escape '\\") + e + "'");
        return {{one, e}};
    }
    consteval std::string ident() {
        std::size_t start = pos;
        while (pos < s.size() && is_word(s[pos])) ++pos;
        return std::string(s.substr(start, pos - start));
    }
    consteval code group() {  // (?:...), (?<name>...) or (?<name:int>...), after the '('
        if (!eat('?')) return fail("bad group");
        int k = -1;  // the group's number, none for (?:...)
        if (!eat(':')) {
            std::string name = eat('<') ? ident() : "", type;
            if (eat(':') && (type = ident()).empty()) return fail("bad group");
            if (name.empty() || is_digit(name[0]) || !eat('>')) return fail("bad group");
            if (!type.empty() && type != "int") return fail("unknown type '" + type + "'");
            if (std::ranges::contains(groups, name, &capture::name)) return fail("duplicate group '" + name + "'");
            k = int(groups.size());
            groups.push_back({name, type == "int"});
        }
        code out = alternatives();
        if (!eat(')')) return fail("unclosed group");
        if (k >= 0) out.insert(out.begin(), {save, 2 * k}), out.push_back({save, 2 * k + 1});
        return out;
    }
};

consteval std::string_view check(std::string_view pattern) {  // "ok", or why the pattern is rejected
    parser p{pattern};
    p.all();
    return p.error.empty() ? "ok" : std::define_static_string(p.error);
}

template <fixed_string P>
struct captures {
    struct type;  // one member per named group, in pattern order
    consteval {
        parser p{P.view()};
        p.all();
        if (!p.error.empty()) return;  // match below rejects the pattern
        std::vector<info> members;
        for (const capture& g : p.groups) {
            info t = g.is_int ? ^^int : ^^std::string_view;
            members.push_back(data_member_spec(g.required ? t : substitute(^^std::optional, {t}), {.name = g.name}));
        }
        define_aggregate(^^type, members);
    }
};

constexpr bool convert(std::string_view s, std::string_view& out) { return out = s, true; }
constexpr bool convert(std::string_view s, int& out) {  // the whole text is one number that fits
    return std::from_chars(s.data(), s.data() + s.size(), out) == std::from_chars_result{s.data() + s.size(), std::errc{}};
}
template <class T>
constexpr bool convert(std::string_view s, std::optional<T>& out) { return convert(s, out.emplace()); }
constexpr bool reads(op kind, int x, char c) {  // whether one, any, digit or word takes the character c
    return kind == any || (kind == one && c == x) || (kind == digit && is_digit(c)) || (kind == word && is_word(c));
}

template <fixed_string P>
    requires(check(P.view()) == "ok")
constexpr std::optional<typename captures<P>::type> match(std::string_view text) {  // the whole text must match
    using C = captures<P>::type;
    static constexpr auto program = std::define_static_array(parser{P.view()}.all());
    static constexpr auto members = std::define_static_array(nonstatic_data_members_of(^^C, std::meta::access_context::unprivileged()));
    std::array<int, 2 * members.size()> caps;  // start and end of each group, or -1
    caps.fill(-1);
    auto run = [&](this auto& self, int pc, std::size_t at) -> bool {
        auto [kind, x, y] = program[pc];
        switch (kind) {
        case done: return at == text.size();
        case jmp: return self(pc + x, at);
        case split: return self(pc + x, at) || self(pc + y, at);
        case save: {  // the old value comes back if the rest fails
            int old = std::exchange(caps[x], int(at));
            if (self(pc + 1, at)) return true;
            caps[x] = old;
            return false;
        }
        default: return at < text.size() && reads(kind, x, text[at]) && self(pc + 1, at + 1);
        }
    };
    if (!run(0, 0)) return std::nullopt;
    C out{};
    template for (int k = 0; constexpr info m : members) {
        int start = caps[k++], end = caps[k++];
        if (start >= 0 && !convert(text.substr(start, end - start), out.[:m:])) return std::nullopt;
    }
    return out;
}
}  // namespace rx

// ---- user code. python: re.fullmatch(r"(?P<year>\d+)-(?P<month>\d\d)", s)["year"], ctre: m.get<"year">()
auto date(std::string_view s) { return rx::match<"(?<year:int>\\d+)-(?<month:int>\\d\\d)-(?<day:int>\\d\\d)">(s); }
auto log_line(std::string_view s) { return rx::match<"(?<level>\\w+) (?<module>\\w+)(?:\\[(?<pid:int>\\d+)\\])?: (?<msg>.*)">(s); }
constexpr auto duration(std::string_view s) { return rx::match<"(?<n:int>\\d+)(?<unit>ms|s|min)">(s); }

using Log = decltype(log_line(""))::value_type;
static_assert(std::is_same_v<decltype(Log::level), std::string_view>);
static_assert(std::is_same_v<decltype(Log::pid), std::optional<int>>);  // inside (...)?, so it can be missing
static_assert(duration("90s")->n == 90 && duration("90s")->unit == "s");  // matching works at compile time too

template <rx::fixed_string P>
constexpr bool compiles = requires { rx::match<P>(""); };
static_assert(compiles<"(?<a>x)|(?<b>y)"> && !compiles<"(?<a>x)|(?<a>y)">);  // a bad pattern does not compile
constexpr std::string_view bad[] = {"(?<a>x)|(?<a>y)", "(?<n:float>\\d+)", "(?<year>\\d+", "a**", "\\q"};

int main() {
    auto [year, month, day] = *date("2026-11-12");
    std::println("day {} month {} year {}, next year is {}", day, month, year, year + 1);
    std::println("99999999999-01-01: {}", date("99999999999-01-01").has_value());  // the year does not fit in int
    for (std::string_view s : {"WARN net[42]: link down", "INFO db: ready", "ERR fs[x]: bad"}) {
        if (auto l = log_line(s))
            std::println("level={} module={} pid={} msg='{}'", l->level, l->module, l->pid ? std::to_string(*l->pid) : "none", l->msg);
        else
            std::println("no match: {}", s);
    }
    template for (constexpr std::string_view pattern : bad) std::println("{:<16} {}", pattern, rx::check(pattern));
}

// output:
// day 12 month 11 year 2026, next year is 2027
// 99999999999-01-01: false
// level=WARN module=net pid=42 msg='link down'
// level=INFO module=db pid=none msg='ready'
// no match: ERR fs[x]: bad
// (?<a>x)|(?<a>y)  duplicate group 'a'
// (?<n:float>\d+)  unknown type 'float'
// (?<year>\d+      unclosed group
// a**              nothing to repeat
// \q               unknown escape '\q'
