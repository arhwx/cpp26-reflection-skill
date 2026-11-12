I want to describe message formats in a tiny schema language right in the source, like `"id:int tags:[string] address:{city:string}"`, and get real nested C++ structs from it, with the field names as member names. I also want a generic printer for values of these types, and a function that reports what is wrong with a schema text.

Everything goes in `namespace schema_dsl`.

Schema language:
- A schema is a sequence of zero or more fields. A field is a name, `:`, and a type. Whitespace (space, tab, newline, carriage return) may appear before and after any name, `:`, type word, bracket, brace and `?`, and is ignored. Fields need no separator other than optional whitespace.
- A name is a letter or `_` followed by letters, digits and `_`. A type word is read the same way, as the longest run of letters, digits and `_`.
- Types:
  - the words `int`, `double`, `bool` and `string` give `int`, `double`, `bool` and `std::string`;
  - `[T]` gives `std::vector<T>`;
  - `{fields}` gives a nested struct whose fields are read with the same rules;
  - any of these followed by one `?` gives `std::optional` of that type, so `[int?]` is `std::vector<std::optional<int>>` and `[bool]?` is `std::optional<std::vector<bool>>`.

API:
- `schema_dsl::schema<"...">`: an alias template whose only template argument is the schema text, given as a string literal (raw string literals included). It is an aggregate struct with one public non-static data member per field, with the field's name and type, in field order, and no other non-static data members. Nested structs follow the same rule, and `{}` and an empty or all-whitespace schema give empty structs. Naming `schema` again with the same text gives the same type. A schema that `problem` rejects must not compile, but the test does not do that.
- `template <class V> std::string to_text(const V& v)`, where `V` is a schema struct or any type that can appear in one:
  - a struct: `{`, then `name: value` for each member in order, joined with `, `, then `}`; an empty struct is `{}`;
  - a vector: `[`, the elements joined with `, `, then `]`;
  - an optional: `null` when empty, otherwise the contained value;
  - a `std::string`: the text in double quotes, no escaping;
  - `int`, `double` and `bool`: as `std::format("{}", v)` writes them.
- `schema_dsl::problem(std::string_view text)`: a function returning `std::string_view` (it may be `consteval`; the test calls it only with string literals). It returns `ok` for a valid schema and otherwise describes the first error met when reading the text from left to right.

Errors, in the form `problem` returns them:
- `expected a field name, found '<c>'`: a field should start here, but the next character `<c>` (after whitespace) cannot start a name. At the top level, the end of the text ends the schema; inside braces, `}` ends the nested struct.
- `missing '}'`: the text ends inside braces where a field or `}` is expected.
- `duplicate field '<name>'`: the field name was already used by an earlier field of the same struct (fields of different structs may share names). This is reported as soon as the name is read.
- `expected ':' after '<name>'`: the next character after the name is not `:`, or the text ends.
- `missing type for '<name>'`: no type word is found where a type should start, for example at the end of the text or at `]`. `<name>` is the field whose type is being read, also inside `[...]`.
- `unknown type '<word>'`: the type word is not one of the four above.
- `missing ']'`: after the element type of a `[`, the next character is not `]`, or the text ends.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <type_traits>
#include <vector>

using User = schema_dsl::schema<R"(
    id:int name:string
    tags:[string]
    address:{city:string zip:int}
    nick:string?
    pets:[{name:string age:int}]
    grid:[[int]]
    boss:{name:string}?
    score: double  active :bool
)">;

using Pair = schema_dsl::schema<"a:{} b:[bool]? c:[int?]">;
using Empty = schema_dsl::schema<"  ">;

template <class T, class U>
constexpr bool same = std::is_same_v<T, U>;

// problem is called with a literal, so it can be evaluated at compile time
#define CHECK(text) std::println("{:<28} {}", text, schema_dsl::problem(text))

int main() {
    std::println("id: {}", same<decltype(User::id), int>);
    std::println("name: {}", same<decltype(User::name), std::string>);
    std::println("tags: {}", same<decltype(User::tags), std::vector<std::string>>);
    std::println("nick: {}", same<decltype(User::nick), std::optional<std::string>>);
    std::println("grid: {}", same<decltype(User::grid), std::vector<std::vector<int>>>);
    using Pet = decltype(User::pets)::value_type;
    using Boss = decltype(User::boss)::value_type;
    std::println("pet age: {}", same<decltype(Pet::age), int>);
    std::println("boss name: {}", same<decltype(Boss::name), std::string>);
    std::println("score: {}", same<decltype(User::score), double>);
    std::println("c: {}", same<decltype(Pair::c), std::vector<std::optional<int>>>);
    std::println("aggregate: {}", std::is_aggregate_v<User> && std::is_aggregate_v<decltype(User::address)>);
    std::println("same text, same type: {}", same<schema_dsl::schema<"x:int y:[double]">, schema_dsl::schema<"x:int y:[double]">>);
    std::println("empty: {}", std::is_empty_v<Empty> && std::is_empty_v<decltype(Pair::a)>);

    User u{
        .id = 7,
        .name = "ada",
        .tags = {"admin", "dev"},
        .address = {.city = "Oslo", .zip = 150},
        .nick = std::nullopt,
        .pets = {{"rex", 3}, {.name = "tom", .age = 5}},
        .grid = {{1, 2}, {}, {3}},
        .boss = std::nullopt,
        .score = 4.5,
        .active = true,
    };
    std::println("{}", schema_dsl::to_text(u));
    u.nick = "countess";
    u.boss.emplace();
    u.boss->name = "grace";
    u.pets.pop_back();
    u.address.city += " west";
    std::println("{}", schema_dsl::to_text(u));
    std::println("{}", schema_dsl::to_text(u.address));

    Pair p{};
    std::println("{}", schema_dsl::to_text(p));
    p.b = std::vector<bool>{true, false};
    p.c = {1, std::nullopt, 3};
    std::println("{}", schema_dsl::to_text(p));
    std::println("{}", schema_dsl::to_text(Empty{}));

    CHECK("id:int name:string");
    CHECK("");
    CHECK("id:int name:strng");
    CHECK("id:int id:double");
    CHECK("a:{x:int} x:int");
    CHECK("a:{x:int x:int}");
    CHECK("id int");
    CHECK("tags:[string");
    CHECK("tags:[string}");
    CHECK("address:{city:string");
    CHECK("a:int?? b:int");
    CHECK("a:");
    CHECK("list:[]");
    CHECK("x:int}");
    CHECK("1a:int");
    CHECK("a:int b:{c:[{d:flaot}]}");
    CHECK("a:intb:int");
}
```

It must print exactly:

```
id: true
name: true
tags: true
nick: true
grid: true
pet age: true
boss name: true
score: true
c: true
aggregate: true
same text, same type: true
empty: true
{id: 7, name: "ada", tags: ["admin", "dev"], address: {city: "Oslo", zip: 150}, nick: null, pets: [{name: "rex", age: 3}, {name: "tom", age: 5}], grid: [[1, 2], [], [3]], boss: null, score: 4.5, active: true}
{id: 7, name: "ada", tags: ["admin", "dev"], address: {city: "Oslo west", zip: 150}, nick: "countess", pets: [{name: "rex", age: 3}], grid: [[1, 2], [], [3]], boss: {name: "grace"}, score: 4.5, active: true}
{city: "Oslo west", zip: 150}
{a: {}, b: null, c: []}
{a: {}, b: [true, false], c: [1, null, 3]}
{}
id:int name:string           ok
                             ok
id:int name:strng            unknown type 'strng'
id:int id:double             duplicate field 'id'
a:{x:int} x:int              ok
a:{x:int x:int}              duplicate field 'x'
id int                       expected ':' after 'id'
tags:[string                 missing ']'
tags:[string}                missing ']'
address:{city:string         missing '}'
a:int?? b:int                expected a field name, found '?'
a:                           missing type for 'a'
list:[]                      missing type for 'list'
x:int}                       expected a field name, found '}'
1a:int                       expected a field name, found '1'
a:int b:{c:[{d:flaot}]}      unknown type 'flaot'
a:intb:int                   unknown type 'intb'
```

Write the complete header to `solution.hpp` in the current directory.
