I parse log lines and file names with small regular expressions, and I am tired of numbered capture groups and string-to-int conversions after every match. I want the pattern to be a template argument, so that every named group becomes a member of a result struct with the group's name, already converted to the right type, and a bad pattern is caught at compile time.

Everything goes in `namespace rx`:
- `rx::match<P>(std::string_view text)`, where the only template argument `P` is a string literal, as in `rx::match<"(?<a>\\d+)">(text)`, returns `std::optional<C>`. It matches the whole of `text` against the pattern and returns the captures, or no value.
- The result type `C` is an aggregate struct with exactly one public non-static data member per named group, in the order in which the groups' opening parentheses appear in the pattern. Each member has the group's name. Its type is `std::string_view` (a view into `text`) for a plain group and `int` for a group written `(?<name:int>...)`. When the group is not required (see below), the type is wrapped in `std::optional`. A pattern without named groups gives an empty struct. The same pattern always gives the same type.
- `consteval std::string_view rx::check(std::string_view pattern)`: `"ok"` for a valid pattern, otherwise the message of its first error, as listed below.
- `rx::match` with a pattern that `rx::check` rejects must not compile.

Pattern syntax (this is the whole language; nothing else is special):
- An ordinary character matches itself. Outside a class the special characters are `\ . [ ( ) * + ? |`; every other character, including `]`, `-`, `^`, `$`, `{` and `}`, is ordinary.
- `.` matches any one character.
- `\d` matches `0`-`9`; `\w` matches `a`-`z`, `A`-`Z`, `0`-`9` and `_`; `\s` matches a space or a tab. A backslash followed by one of `\ . [ ] ( ) * + ? | -` matches that character. A backslash followed by anything else is the error `unknown escape '\X'`, where `X` is that character. A backslash at the very end is the error `trailing backslash`.
- `[...]` matches one character from a set. Inside, the first `]` ends the class (so `[]` matches nothing). The backslash escapes above work inside a class too. A plain character `x` followed by `-` and then a character `y` that is neither `]` nor a backslash adds the range `x`-`y`; any other `-` is an ordinary member. A class without a closing `]` is the error `unclosed class`.
- `(?:R)` is a group without a name. `(?<name>R)` and `(?<name:type>R)` are named groups. A name is one or more of `a`-`z`, `A`-`Z`, `0`-`9`, `_`, not starting with a digit. A type is one or more letters, and the only known type is `int`. Any other `(` (including `(x)` and `()`), an empty or invalid name, an empty type, or a missing `>` is the error `bad group`. An unknown type is the error `unknown type 'T'`. A name used by an earlier group is the error `duplicate group 'name'`. A group that is still open at the end of the pattern is the error `unclosed group`; a `)` with no open group is the error `unmatched ')'`.
- `*`, `+` and `?` after a character, `.`, escape, class or group repeat it 0 or more, 1 or more, or 0 or 1 times. All three are greedy. A quantifier at the start of the pattern, right after `(?:`, `(?<...>` or `|`, or right after another quantifier is the error `nothing to repeat`.
- `A|B|C` tries the alternatives in order. `|` has the lowest precedence, inside a group or at the top level, and alternatives may be empty.
- Errors are found by reading the pattern once from left to right; the first one reached is reported. Inside a group header, `bad group` is checked first, then `unknown type`, then `duplicate group`.

Matching:
- The pattern must match the whole text. When several ways to match exist, the result is the first one found by a backtracking search that prefers more repetitions for every quantifier and earlier alternatives for every `|`.
- A group's text is what it matched. When it matched more than once inside a repetition, it keeps the text of its last match.
- A group is required when it is not inside (at any depth) the operand of a `*` or `?`, and not inside any alternative of a `|` that has two or more alternatives. Only those get a plain type. A non-required group that did not take part in the match is an empty optional.
- An `int` group's text must consist of an optional `-` followed by one or more decimal digits, with a value that fits in `int`. Otherwise `match` returns no value, even though the pattern matched.

Tests never repeat something that can match the empty string.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <format>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>

template <class T>
std::string show(const std::optional<T>& v) {
    return v ? std::format("{}", *v) : "-";
}

auto date(std::string_view s) { return rx::match<"(?<year:int>\\d+)-(?<month:int>\\d\\d)-(?<day:int>\\d\\d)">(s); }
auto log_line(std::string_view s) {
    return rx::match<"(?<level>[A-Z]+) (?<module>\\w+)(?:\\[(?<pid:int>\\d+)\\])?: (?<msg>.*)">(s);
}
auto path(std::string_view s) { return rx::match<"(?<dir>.*)/(?<file>[\\w.]+)">(s); }
auto duration(std::string_view s) { return rx::match<"(?<num:int>-?\\d+)(?<unit>ms|s|m)">(s); }
auto literal(std::string_view s) { return rx::match<"(?<hex>0x[0-9a-f]+)|(?<dec:int>\\d+)">(s); }
auto list(std::string_view s) { return rx::match<"(?<first>\\w+)(?:,(?<last>\\w+))*">(s); }
auto file(std::string_view s) { return rx::match<"(?:(?<stem>\\w+)\\.)+(?<ext>\\w+)">(s); }
auto number(std::string_view s) { return rx::match<"(?<n:int>[\\w-]+)">(s); }
auto decimal(std::string_view s) { return rx::match<"\\d+(?:\\.\\d+)?">(s); }

using Date = decltype(date(""))::value_type;
using Log = decltype(log_line(""))::value_type;
using Literal = decltype(literal(""))::value_type;
using File = decltype(file(""))::value_type;
static_assert(std::is_same_v<decltype(Date::year), int>);
static_assert(std::is_same_v<decltype(Log::level), std::string_view>);
static_assert(std::is_same_v<decltype(Log::pid), std::optional<int>>);
static_assert(std::is_same_v<decltype(Log::msg), std::string_view>);
static_assert(std::is_same_v<decltype(Literal::hex), std::optional<std::string_view>>);
static_assert(std::is_same_v<decltype(Literal::dec), std::optional<int>>);
static_assert(std::is_same_v<decltype(File::stem), std::string_view>);
static_assert(std::is_same_v<decltype(list("")->last), std::optional<std::string_view>>);
static_assert(std::is_empty_v<decltype(decimal(""))::value_type>);

int main() {
    if (auto d = date("2026-11-05")) {
        auto [y, m, day] = *d;
        std::println("date {}/{}/{} sum={}", day, m, y, y + m + day);
    }
    std::println("date 2026-1-05: {}", date("2026-1-05").has_value());

    for (std::string_view s : {"WARN net[42]: link down", "INFO db: ready", "info db: ready", "ERR fs[x]: bad"}) {
        if (auto l = log_line(s))
            std::println("log level={} module={} pid={} msg='{}'", l->level, l->module, show(l->pid), l->msg);
        else
            std::println("log no match: {}", s);
    }

    for (std::string_view s : {"usr/local/lib.so", "/x", "a/b/", "noslash"}) {
        if (auto p = path(s)) std::println("path dir='{}' file='{}'", p->dir, p->file);
        else std::println("path no match: {}", s);
    }

    for (std::string_view s : {"250ms", "5m", "-12s", "3h"}) {
        if (auto d = duration(s)) std::println("duration {} {}", d->num, d->unit);
        else std::println("duration no match: {}", s);
    }

    for (std::string_view s : {"0x1f", "42", "0x", "0x2G"}) {
        if (auto l = literal(s)) std::println("literal hex={} dec={}", show(l->hex), show(l->dec));
        else std::println("literal no match: {}", s);
    }

    for (std::string_view s : {"a,bb,c", "solo", "a,,b"}) {
        if (auto l = list(s)) std::println("list first={} last={}", l->first, show(l->last));
        else std::println("list no match: {}", s);
    }

    for (std::string_view s : {"archive.tar.gz", "notes.txt", "README"}) {
        if (auto f = file(s)) std::println("file stem={} ext={}", f->stem, f->ext);
        else std::println("file no match: {}", s);
    }

    for (std::string_view s : {"123", "-7", "12a", "99999999999", "--1"}) {
        if (auto n = number(s)) std::println("number {}", n->n);
        else std::println("number no value: {}", s);
    }

    for (std::string_view s : {"3.14", "3.", "17"}) std::println("decimal {}: {}", s, decimal(s).has_value());

    constexpr std::string_view errors[] = {
        rx::check("(?<n:int>\\d+)"),     rx::check("(?:a|)?b*"),          rx::check("(?<a>x)|(?<a>y)"),
        rx::check("(?<a>x"),             rx::check("a)b"),                rx::check("*a"),
        rx::check("a**"),                rx::check("a|+"),                rx::check("(x)"),
        rx::check("(?<1a>x)"),           rx::check("(?<n:float>\\d)"),    rx::check("\\q"),
        rx::check("[a-z"),               rx::check("(?<a>[)])"),          rx::check("a\\"),
    };
    for (std::string_view e : errors) std::println("check: {}", e);
}
```

It must print exactly:

```
date 5/11/2026 sum=2042
date 2026-1-05: false
log level=WARN module=net pid=42 msg='link down'
log level=INFO module=db pid=- msg='ready'
log no match: info db: ready
log no match: ERR fs[x]: bad
path dir='usr/local' file='lib.so'
path dir='' file='x'
path no match: a/b/
path no match: noslash
duration 250 ms
duration 5 m
duration -12 s
duration no match: 3h
literal hex=0x1f dec=-
literal hex=- dec=42
literal no match: 0x
literal no match: 0x2G
list first=a last=c
list first=solo last=-
list no match: a,,b
file stem=tar ext=gz
file stem=notes ext=txt
file no match: README
number 123
number -7
number no value: 12a
number no value: 99999999999
number no value: --1
decimal 3.14: true
decimal 3.: false
decimal 17: true
check: ok
check: ok
check: duplicate group 'a'
check: unclosed group
check: unmatched ')'
check: nothing to repeat
check: nothing to repeat
check: nothing to repeat
check: bad group
check: bad group
check: unknown type 'float'
check: unknown escape '\q'
check: unclosed class
check: ok
check: trailing backslash
```

Write the complete header to `solution.hpp` in the current directory.
