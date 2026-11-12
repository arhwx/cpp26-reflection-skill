#include "solution.hpp"

#include <print>
#include <string>
#include <type_traits>

template <class T>
const char* kind() {
    if constexpr (std::is_same_v<T, int>)
        return "int";
    else if constexpr (std::is_same_v<T, double>)
        return "double";
    else if constexpr (std::is_same_v<T, char>)
        return "char";
    else if constexpr (std::is_same_v<T, std::string>)
        return "string";
    else
        return "other";
}

template <fmt::fixed_string F>
concept valid = requires { typename fmt::args<F>; };

template <fmt::fixed_string F, class T>
concept accepts = requires(T v) { fmt::format<F>({.a = v}); };

using Row = fmt::args<"{item:<6}|{qty:>4d}|{price:8.2f}">;
using Mixed = fmt::args<"{i:d}{h:#x}{o:o}{f:.3f}{e:e}{g:g}{c:c}{s:s}{t}{w:>5}{u:}">;

int main() {
    std::println("[{}]", fmt::format<"{name} is {age:d}">({.name = "ada", .age = 36}));
    std::println("[{}]", fmt::format<"{item:<6}|{qty:>4d}|{price:8.2f}">({.item = "apple", .qty = 3, .price = 1.5}));
    std::println("[{}]", fmt::format<"{n:d} is {n:#x} in hex and {n:08b} in binary">({.n = 37}));
    std::println("[{}]", fmt::format<"{initial:c}. {last:s}">({.initial = 'A', .last = "Lovelace"}));
    std::println("[{}]", fmt::format<"{x:e} {x:g} {x:+.1f}">({.x = 12345.678}));
    std::println("[{}]", fmt::format<"{{{word}}} and {{word}}">({.word = "braces"}));
    std::println("[{}]", fmt::format<"{a}-{b:d}-{c:.2f}">({.a = "only a"}));
    std::println("[{}]", fmt::format<"{title:*^11}|{title:.3}">({.title = "menu"}));
    std::println("[{}]", fmt::format<"{long_name_2}{_x:X}">({.long_name_2 = "id=", ._x = 48879}));
    std::println("[{}]", fmt::format<"plain text">({}));
    std::println("[{}]", fmt::format<"">({}));

    Row bill{.item = "pear", .qty = 2, .price = 0.75};
    bill.qty += 10;
    std::println("[{}]", fmt::format<"{item:<6}|{qty:>4d}|{price:8.2f}">(bill));
    auto& [item, qty, price] = bill;
    std::println("row members: {} {} {}", kind<std::remove_cvref_t<decltype(item)>>(), kind<decltype(bill.qty)>(),
                 kind<decltype(bill.price)>());
    std::println("price member: {}", price);

    std::println("mixed: {} {} {} {} {} {} {} {} {} {} {}", kind<decltype(Mixed::i)>(), kind<decltype(Mixed::h)>(),
                 kind<decltype(Mixed::o)>(), kind<decltype(Mixed::f)>(), kind<decltype(Mixed::e)>(),
                 kind<decltype(Mixed::g)>(), kind<decltype(Mixed::c)>(), kind<decltype(Mixed::s)>(),
                 kind<decltype(Mixed::t)>(), kind<decltype(Mixed::w)>(), kind<decltype(Mixed::u)>());
    std::println("same type for the same format: {}",
                 std::is_same_v<Row, fmt::args<"{item:<6}|{qty:>4d}|{price:8.2f}">>);
    std::println("different format, different type: {}", std::is_same_v<Row, fmt::args<"{item:<6}|{qty:>4d}">>);

    std::println("valid fields: {}", valid<"{name} is {age:d}">);
    std::println("valid repeat with another spec: {}", valid<"{a:d} {a:+5d}">);
    std::println("valid escapes only: {}", valid<"{{a}}">);
    std::println("repeat with another type: {}", valid<"{a:d} {a:s}">);
    std::println("unclosed brace: {}", valid<"{a">);
    std::println("stray close brace: {}", valid<"a}">);
    std::println("empty name: {}", valid<"{}">);
    std::println("empty name with spec: {}", valid<"{:d}">);
    std::println("name starts with digit: {}", valid<"{2x}">);
    std::println("name with dash: {}", valid<"{a-b}">);
    std::println("int for int: {}", accepts<"{a:d}", int>);
    std::println("string for int: {}", accepts<"{a:d}", std::string>);
    std::println("char pointer for string: {}", accepts<"{a}", const char*>);
    std::println("int for string: {}", accepts<"{a:>3}", int>);
    std::println("misspelled member: {}", accepts<"{b:d}", int>);
}
