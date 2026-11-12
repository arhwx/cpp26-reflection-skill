I want a format function whose argument type comes from the format string, like printf in a dependently typed language: `fmt::format<"{name} is {age:d}">({.name = "ada", .age = 36})`. The placeholders name the fields and their presentation types decide the field types, so a wrong or missing name, or a value of the wrong type, is a compile error.

API, in namespace `fmt`:
- `template <std::size_t N> struct fixed_string`: a structural type constructible from a string literal, so that `template <fmt::fixed_string F>` accepts `"..."` as a template argument.
- `template <fixed_string F> using args = ...;`: an aggregate with one public data member per distinct placeholder name in `F`, in order of first appearance, named after the placeholder. It has no other members, and its members have no default member initializers. The same format string always gives the same type. For an invalid `F`, `args<F>` must not name a type, in a way a requires-expression detects (`requires { typename fmt::args<F>; }` yields `false`, it is not a hard error).
- `template <fixed_string F> std::string format(const args<F>& values)`: returns the formatted text. For an invalid `F` it is not a viable function (a requires-expression around the call yields `false`).

Format strings:
- `{{` stands for `{` and `}}` for `}`, read left to right. Any other `}` outside a placeholder makes the format invalid.
- A placeholder is `{name}` or `{name:spec}`. It ends at the first `}` after its `{`; if there is none, the format is invalid. The name is the text before the first `:`, and the spec is the text after it, which may be empty.
- A name starts with an ASCII letter or `_` and continues with ASCII letters, digits and `_`. Any other name, including an empty one, makes the format invalid.
- The member type comes from the last character of the spec: `d`, `x`, `X`, `o` or `b` give `int`; `f`, `e` or `g` give `double`; `c` gives `char`; any other character, or an empty spec, gives `std::string`.
- A name may appear more than once, with the same or another spec. If two of its specs give different types, the format is invalid.

Output:
- Literal text is copied, with `{{` and `}}` turned into single braces.
- A placeholder is replaced by `std::format` of its member's value, using the replacement field `{:spec}`. The test only uses specs that are valid for the member's type.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
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
```

It must print exactly:

```
[ada is 36]
[apple |   3|    1.50]
[37 is 0x25 in hex and 00100101 in binary]
[A. Lovelace]
[1.234568e+04 12345.7 +12345.7]
[{braces} and {word}]
[only a-0-0.00]
[***menu****|men]
[id=BEEF]
[plain text]
[]
[pear  |  12|    0.75]
row members: string int double
price member: 0.75
mixed: int int int double double double char string string string string
same type for the same format: true
different format, different type: false
valid fields: true
valid repeat with another spec: true
valid escapes only: true
repeat with another type: false
unclosed brace: false
stray close brace: false
empty name: false
empty name with spec: false
name starts with digit: false
name with dash: false
int for int: true
string for int: false
char pointer for string: true
int for string: false
misspelled member: false
```

Write the complete header to `solution.hpp` in the current directory.
