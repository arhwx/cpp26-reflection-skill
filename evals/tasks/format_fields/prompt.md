Our status messages are built from structs, and I want the message templates checked by the compiler. `format_fields<"{name} is {age} years">(person)` should fill in the fields, and a typo in a field name should not compile.

API:
- `template <fixed_string Fmt, class T> std::string format_fields(const T& value)`: returns `Fmt` with each placeholder replaced by a field of `value`. `fixed_string` is a type you define so that a plain string literal can be the template argument, as in the test.

Format rules:
- `{field}` is replaced by the public non-static data member of `T` called `field`, resolved at compile time. A field can be used any number of times, and placeholders can be next to each other.
- `{{` writes `{` and `}}` writes `}`. All other text is copied as is, and an empty format gives an empty string.
- Integer members (`int`, `long`, `unsigned` and so on) are written with `std::to_string`. Members of type `std::string`, `std::string_view` or `const char*` are written as is. `T` may have members of other types, but they can't be used in a format.

Invalid formats:
- A format is invalid for `T` if a placeholder names anything other than a public non-static data member of `T` with one of the types above, if a `{` has no closing `}`, or if a `}` is not part of a placeholder or of `}}`.
- An invalid format must not compile, and the check must be part of the constraints of `format_fields`: with an invalid format, `format_fields<...>(value)` does not satisfy its constraints, so a requires-expression sees the call as invalid. The test checks this with concepts.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

struct Person {
    std::string name;
    int age;
    std::string_view city;
    long id;
    std::vector<int> scores;
};

struct Pet {
    int legs;
    std::string name;
};

struct Item {
    int count;
    int count2;
    std::string count_label;
};

class Account {
public:
    Account(std::string o, unsigned n, const char* c) : owner(std::move(o)), number(n), currency(c) {}
    bool check(int code) const { return code == pin; }

    std::string owner;
    unsigned number;
    const char* currency;

private:
    int pin = 1234;
};

template <class T>
concept valid_field = requires(const T& v) { format_fields<"{name}">(v); };
template <class T>
concept unknown_field = requires(const T& v) { format_fields<"{nope}">(v); };
template <class T>
concept vector_field = requires(const T& v) { format_fields<"{scores}">(v); };
template <class T>
concept private_field = requires(const T& v) { format_fields<"{pin}">(v); };
template <class T>
concept unmatched_open = requires(const T& v) { format_fields<"{name">(v); };
template <class T>
concept stray_close = requires(const T& v) { format_fields<"name}">(v); };

int main() {
    Person p{"Ann", 31, "Oslo", -7, {90, 85}};
    std::println("[{}]", format_fields<"{name} is {age} years">(p));
    std::println("[{}]", format_fields<"{name} and {name}">(p));
    std::println("[{}]", format_fields<"{name}{age}{city}">(p));
    std::println("[{}]", format_fields<"{{{name}}}">(p));
    std::println("[{}]", format_fields<"{{name}} = {name}, a}}b">(p));
    std::println("[{}]", format_fields<"#{id} from {city}">(p));
    std::println("[{}]", format_fields<"{age}">(p));
    std::println("[{}]", format_fields<"no fields">(p));
    std::println("[{}]", format_fields<"">(p));

    Pet pet{4, "Rex"};
    std::println("[{}]", format_fields<"{name} has {legs} legs">(pet));

    Item item{3, 12, "boxes"};
    std::println("[{}]", format_fields<"{count2} {count_label}, {count} left">(item));

    Account acc("Bob", 42u, "EUR");
    std::println("[{}]", format_fields<"{owner}: account {number} in {currency}">(acc));

    std::println("valid field: {}", valid_field<Person>);
    std::println("unknown field: {}", unknown_field<Person>);
    std::println("vector field: {}", vector_field<Person>);
    std::println("private field: {}", private_field<Account>);
    std::println("unmatched open brace: {}", unmatched_open<Person>);
    std::println("stray close brace: {}", stray_close<Person>);
}
```

It must print exactly:

```
[Ann is 31 years]
[Ann and Ann]
[Ann31Oslo]
[{Ann}]
[{name} = Ann, a}b]
[#-7 from Oslo]
[31]
[no fields]
[]
[Rex has 4 legs]
[12 boxes, 3 left]
[Bob: account 42 in EUR]
valid field: true
unknown field: false
vector field: false
private field: false
unmatched open brace: false
stray close brace: false
```

Write the complete header to `solution.hpp` in the current directory.
