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
