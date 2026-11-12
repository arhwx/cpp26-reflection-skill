# Recipes

Complete programs. Each one compiles as standard C++26 and prints the output shown below it.

## Contents

- [Enum to string and back](#enum-to-string-and-back)
- [Printing structs, nested structs and bases](#printing-structs-nested-structs-and-bases)
- [Field names as a constexpr array](#field-names-as-a-constexpr-array)
- [Finding a member by name at compile time](#finding-a-member-by-name-at-compile-time)
- [Setting a field by a runtime name](#setting-a-field-by-a-runtime-name)
- [Serialization with annotations](#serialization-with-annotations)
- [Struct of arrays](#struct-of-arrays)
- [Function parameters](#function-parameters)
- [Transforming a type list](#transforming-a-type-list)

## Enum to string and back

`enumerators_of` takes no access context. `[:e:]` is the enumerator value.

```cpp
#include <meta>
#include <optional>
#include <print>
#include <string_view>
#include <type_traits>

template <class E>
    requires std::is_enum_v<E>
constexpr std::string_view enum_to_string(E value) {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (value == [:e:]) return std::meta::identifier_of(e);
    }
    return {};
}

template <class E>
    requires std::is_enum_v<E>
constexpr std::optional<E> enum_from_string(std::string_view name) {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (name == std::meta::identifier_of(e)) return [:e:];
    }
    return std::nullopt;
}

enum class Color { red, green = 5, blue };

static_assert(enum_to_string(Color::green) == "green");
static_assert(enum_from_string<Color>("blue") == Color::blue);

int main() {
    std::println("{}", enum_to_string(Color::blue));
    std::println("{}", enum_to_string(static_cast<Color>(1)).empty());
    std::println("{}", static_cast<int>(*enum_from_string<Color>("green")));
    std::println("{}", enum_from_string<Color>("pink").has_value());
}
```

```text
blue
true
5
false
```

## Printing structs, nested structs and bases

Bases come first, through a cast to the base type. The access context is `current()` at namespace scope, so only public members and bases are printed. Use `access_context::unchecked()` to include private ones.

```cpp
#include <format>
#include <meta>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>

template <class T>
void append(std::string& out, const T& value);

template <class T>
void append_members(std::string& out, const T& obj, bool& first) {
    constexpr auto ctx = std::meta::access_context::current();
    template for (constexpr auto b : std::define_static_array(std::meta::bases_of(^^T, ctx))) {
        using Base = [:std::meta::type_of(b):];
        append_members(out, static_cast<const Base&>(obj), first);
    }
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
        if (!first) out += ", ";
        first = false;
        out += std::meta::identifier_of(m);
        out += '=';
        append(out, obj.[:m:]);
    }
}

template <class T>
void append(std::string& out, const T& value) {
    if constexpr (std::is_arithmetic_v<T>) {
        out += std::format("{}", value);
    } else if constexpr (std::is_convertible_v<const T&, std::string_view>) {
        out += std::format("\"{}\"", std::string_view(value));
    } else {
        out += '{';
        bool first = true;
        append_members(out, value, first);
        out += '}';
    }
}

template <class T>
std::string to_string(const T& value) {
    std::string out;
    append(out, value);
    return out;
}

struct Inner { int a; };
struct Base { int id; };
struct Item : Base {
    double price;
    std::string name;
    Inner inner;
};

int main() {
    std::println("{}", to_string(Item{{7}, 2.5, "pen", {3}}));
}
```

```text
{id=7, price=2.5, name="pen", inner={a=3}}
```

## Field names as a constexpr array

A plain `for` loop is fine inside a `consteval` function when nothing is spliced. The returned array holds `string_view`s into static storage, so it can be a `constexpr` variable and be used at run time.

```cpp
#include <array>
#include <meta>
#include <print>
#include <string_view>

template <class T>
consteval auto field_names() {
    constexpr auto ctx = std::meta::access_context::current();
    constexpr std::size_t n = std::meta::nonstatic_data_members_of(^^T, ctx).size();
    std::array<std::string_view, n> names{};
    std::size_t i = 0;
    for (std::meta::info m : std::meta::nonstatic_data_members_of(^^T, ctx)) names[i++] = std::meta::identifier_of(m);
    return names;
}

struct Point { int x; int y; int z; };

constexpr auto names = field_names<Point>();
static_assert(names.size() == 3 && names[1] == "y");

int main() {
    for (std::string_view n : names) std::println("{}", n);
}
```

```text
x
y
z
```

## Finding a member by name at compile time

A `consteval` function can return a reflection. Its result must be used in a constant context, here a splice.

```cpp
#include <meta>
#include <print>
#include <string_view>

consteval std::meta::info member_named(std::meta::info type, std::string_view name,
                                       std::meta::access_context ctx = std::meta::access_context::current()) {
    for (std::meta::info m : std::meta::nonstatic_data_members_of(type, ctx))
        if (std::meta::has_identifier(m) && std::meta::identifier_of(m) == name) return m;
    throw std::meta::exception("no such member", ^^member_named);
}

struct Config { int port; bool verbose; };

int main() {
    Config c{8080, true};
    c.[:member_named(^^Config, "port"):] += 1;
    std::println("{}", c.port);
}
```

```text
8081
```

## Setting a field by a runtime name

There is no runtime reflection. Generate one comparison per member with `template for`, and branch on the member type with `if constexpr`.

```cpp
#include <charconv>
#include <meta>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>

template <class T>
bool set_field(T& obj, std::string_view name, std::string_view text) {
    constexpr auto ctx = std::meta::access_context::current();
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
        if (name == std::meta::identifier_of(m)) {
            using M = [:std::meta::type_of(m):];
            if constexpr (std::is_same_v<M, bool>) {
                obj.[:m:] = text == "true";
                return true;
            } else if constexpr (std::is_integral_v<M>) {
                auto [_, ec] = std::from_chars(text.data(), text.data() + text.size(), obj.[:m:]);
                return ec == std::errc{};
            } else if constexpr (std::is_same_v<M, std::string>) {
                obj.[:m:] = text;
                return true;
            } else {
                return false;
            }
        }
    }
    return false;
}

struct Options {
    int port = 80;
    bool verbose = false;
    std::string host = "localhost";
};

int main() {
    Options o;
    set_field(o, "port", "8080");
    set_field(o, "verbose", "true");
    set_field(o, "host", "example.org");
    bool unknown = set_field(o, "color", "red");
    std::println("{} {} {} {}", o.port, o.verbose, o.host, unknown);
}
```

```text
8080 true example.org false
```

## Serialization with annotations

Annotation values must be structural, so `rename` stores a pointer from `std::define_static_string`. The type of a class-type annotation is `const A`, hence `remove_const`.

```cpp
#include <format>
#include <meta>
#include <print>
#include <string>
#include <string_view>
#include <type_traits>

namespace json {

struct rename {
    const char* name;
    consteval rename(std::string_view n) : name(std::define_static_string(n)) {}
};
struct skip_t {};
inline constexpr skip_t skip{};

template <class A>
consteval std::meta::info find_annotation(std::meta::info r) {
    for (std::meta::info a : std::meta::annotations_of(r))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^A) return a;
    return {};
}

template <class T>
void write(std::string& out, const T& value) {
    if constexpr (std::is_same_v<T, bool>) {
        out += value ? "true" : "false";
    } else if constexpr (std::is_arithmetic_v<T>) {
        out += std::format("{}", value);
    } else if constexpr (std::is_convertible_v<const T&, std::string_view>) {
        out += std::format("\"{}\"", std::string_view(value));
    } else {
        out += '{';
        bool first = true;
        constexpr auto ctx = std::meta::access_context::current();
        template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
            if constexpr (find_annotation<skip_t>(m) == std::meta::info{}) {
                if (!first) out += ',';
                first = false;
                constexpr std::meta::info r = find_annotation<rename>(m);
                if constexpr (r != std::meta::info{})
                    out += std::format("\"{}\":", std::meta::extract<rename>(r).name);
                else
                    out += std::format("\"{}\":", std::meta::identifier_of(m));
                write(out, value.[:m:]);
            }
        }
        out += '}';
    }
}

template <class T>
std::string to_json(const T& value) {
    std::string out;
    write(out, value);
    return out;
}

}  // namespace json

struct Address { std::string city; };
struct User {
    [[=json::rename("user_id")]] int id;
    std::string name;
    [[=json::skip]] std::string password;
    bool active;
    Address address;
};

int main() {
    std::println("{}", json::to_json(User{42, "ann", "secret", true, {"Oslo"}}));
}
```

```text
{"user_id":42,"name":"ann","active":true,"address":{"city":"Oslo"}}
```

## Struct of arrays

`define_aggregate` runs in a `consteval` block and completes a class that is declared but not defined. To generate a type per template argument, declare a nested class in a class template.

```cpp
#include <meta>
#include <print>
#include <ranges>
#include <vector>

template <class T>
struct soa_impl {
    struct type;
    consteval {
        std::vector<std::meta::info> specs;
        for (std::meta::info m : std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current())) {
            std::meta::info column = std::meta::substitute(^^std::vector, {std::meta::type_of(m)});
            specs.push_back(std::meta::data_member_spec(column, {.name = std::meta::identifier_of(m)}));
        }
        std::meta::define_aggregate(^^type, specs);
    }
};

template <class T>
using soa = soa_impl<T>::type;

template <class T>
void push_back(soa<T>& s, const T& value) {
    constexpr auto ctx = std::meta::access_context::current();
    static constexpr auto src = std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx));
    static constexpr auto dst = std::define_static_array(std::meta::nonstatic_data_members_of(^^soa<T>, ctx));
    template for (constexpr std::size_t i : std::define_static_array(std::views::iota(0uz, src.size())))
        s.[:dst[i]:].push_back(value.[:src[i]:]);
}

struct Particle { float x; float y; int id; };

int main() {
    soa<Particle> ps;
    for (int i = 0; i < 3; ++i) push_back(ps, Particle{i * 1.5f, -i * 1.0f, i});
    std::println("x: {}", ps.x);
    std::println("id: {}", ps.id);
}
```

```text
x: [0, 1.5, 3]
id: [0, 1, 2]
```

## Function parameters

`parameters_of` gives parameter reflections. A parameter name is available only if every declaration of the function uses the same name, so check `has_identifier`. Splice the function reflection to call it.

```cpp
#include <meta>
#include <print>
#include <string>

int add(int lhs, int rhs) { return lhs + rhs; }
void log_line(const char* text, int);

template <std::meta::info F>
std::string signature() {
    std::string out(std::meta::identifier_of(F));
    out += '(';
    bool first = true;
    template for (constexpr auto p : std::define_static_array(std::meta::parameters_of(F))) {
        if (!first) out += ", ";
        first = false;
        if constexpr (std::meta::has_identifier(p))
            out += std::meta::identifier_of(p);
        else
            out += '_';
    }
    return out + ')';
}

int main() {
    std::println("{}", signature<^^add>());
    std::println("{}", signature<^^log_line>());
    constexpr std::size_t n = std::meta::parameters_of(^^add).size();
    std::println("{} params, add(2, 3) = {}", n, [:^^add:](2, 3));
}
```

```text
add(lhs, rhs)
log_line(text, _)
2 params, add(2, 3) = 5
```

## Transforming a type list

`template_arguments_of` takes a specialization apart, and `substitute` builds a new one. Use `dealias` when the input may be an alias. To test with any variable-template trait, use `std::meta::extract<bool>(std::meta::substitute(^^std::is_integral_v, {t}))`.

```cpp
#include <meta>
#include <print>
#include <tuple>
#include <type_traits>
#include <vector>

template <class Tuple>
consteval std::meta::info integral_only() {
    std::vector<std::meta::info> kept;
    for (std::meta::info t : std::meta::template_arguments_of(std::meta::dealias(^^Tuple)))
        if (std::meta::is_integral_type(t)) kept.push_back(t);
    return std::meta::substitute(^^std::tuple, kept);
}

template <class Tuple>
using integral_only_t = [:integral_only<Tuple>():];

using Mixed = std::tuple<int, double, long, char, float>;
static_assert(std::is_same_v<integral_only_t<Mixed>, std::tuple<int, long, char>>);

int main() {
    std::println("{}", std::tuple_size_v<integral_only_t<Mixed>>);
}
```

```text
3
```
