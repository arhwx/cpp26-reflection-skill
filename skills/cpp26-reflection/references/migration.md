# Migrating to reflection

## Contents

- [When not to use reflection](#when-not-to-use-reflection)
- [X-macros](#x-macros)
- [magic_enum](#magic_enum)
- [Boost.PFR](#boostpfr)
- [Boost.Describe](#boostdescribe)
- [Registration patterns](#registration-patterns)
- [Type lists and index sequences](#type-lists-and-index-sequences)
- [Keeping a fallback](#keeping-a-fallback)

## When not to use reflection

- The code must build with compilers that do not support reflection. Keep the existing technique, or add reflection behind a feature test and keep the old path as the fallback.
- A hand-written function is shorter and clearer, for example comparing two fields once.
- You need information at run time about types that are not known at compile time, such as plugins loaded from shared libraries. Reflection can generate a registry, but only for types the compiler sees.
- The generated code would be very large, such as a `template for` over thousands of members in a hot header. Measure build time first.

When migrating, keep the public API of the old code so callers do not change.

## X-macros

Before: every enumerator is listed in a macro so a name table can be generated.

```cpp
#include <string_view>

#define COLOR_LIST(X) X(red) X(green) X(blue)

enum class Color {
#define X(name) name,
    COLOR_LIST(X)
#undef X
};

constexpr std::string_view color_name(Color c) {
    switch (c) {
#define X(name) case Color::name: return #name;
        COLOR_LIST(X)
#undef X
    }
    return {};
}

static_assert(color_name(Color::green) == "green");
```

After: declare the enum normally and generate the lookup with `enumerators_of`, as in the enum recipe in [recipes.md](recipes.md). Delete the list macro. For bit flags, test each enumerator against the value:

```cpp
#include <meta>
#include <print>
#include <string>
#include <type_traits>

enum class Perm : unsigned { read = 1, write = 2, exec = 4 };

template <class E>
std::string flags_to_string(E value) {
    using U = std::underlying_type_t<E>;
    std::string out;
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (static_cast<U>([:e:]) != 0 && (static_cast<U>(value) & static_cast<U>([:e:])) == static_cast<U>([:e:])) {
            if (!out.empty()) out += '|';
            out += std::meta::identifier_of(e);
        }
    }
    return out.empty() ? "none" : out;
}

int main() {
    std::println("{}", flags_to_string(static_cast<Perm>(5)));
    std::println("{}", flags_to_string(Perm{}));
}
```

```text
read|exec
none
```

## magic_enum

magic_enum finds enumerators by probing a range of values, `[-128, 128]` by default, and parsing compiler-specific function names. Reflection lists the declared enumerators directly, with no range limit.

| magic_enum | C++26 |
|---|---|
| `enum_name(v)` | loop over `enumerators_of(^^E)`, compare with `[:e:]`, return `identifier_of(e)` |
| `enum_cast<E>(name)` | same loop, compare `name` with `identifier_of(e)`, return `[:e:]` |
| `enum_count<E>()` | `std::meta::enumerators_of(^^E).size()` in a `constexpr` variable |
| `enum_values<E>()` | a `consteval` function filling `std::array<E, n>` with `[:e:]` in a `template for` |
| `enum_names<E>()` | a `consteval` function filling `std::array<std::string_view, n>` |
| `enum_contains(v)` | the name loop, testing for a match |

Behavior differences: enumerators with the same value are all listed, in declaration order, and a name lookup returns the first one that matches. Values outside any range are handled the same as values inside it.

## Boost.PFR

PFR works only on aggregates and finds fields through structured bindings and aggregate initialization. Reflection works on any class, sees bases, and gives real member names.

| Boost.PFR | C++26 |
|---|---|
| `pfr::for_each_field(obj, f)` | `template for (constexpr auto m : std::define_static_array(nonstatic_data_members_of(^^T, ctx))) f(obj.[:m:]);` |
| `pfr::get<I>(obj)` | `obj.[:members[I]:]` with `static constexpr auto members = std::define_static_array(...)` |
| `pfr::tuple_size_v<T>` | `nonstatic_data_members_of(^^T, ctx).size()` in a `constexpr` variable |
| `pfr::get_name<I, T>()` | `identifier_of(members[I])` |
| `pfr::eq`, `pfr::hash_value` | a `template for` over the members, as below |

```cpp
#include <cstddef>
#include <functional>
#include <meta>
#include <print>
#include <string>

template <class T>
bool members_equal(const T& a, const T& b) {
    constexpr auto ctx = std::meta::access_context::current();
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
        if (!(a.[:m:] == b.[:m:])) return false;
    }
    return true;
}

template <class T>
std::size_t members_hash(const T& value) {
    std::size_t h = 0;
    constexpr auto ctx = std::meta::access_context::current();
    template for (constexpr auto m : std::define_static_array(std::meta::nonstatic_data_members_of(^^T, ctx))) {
        using M = [:std::meta::type_of(m):];
        h ^= std::hash<M>{}(value.[:m:]) + 0x9e3779b97f4a7c15uz + (h << 6) + (h >> 2);
    }
    return h;
}

struct Key { int id; std::string name; };

int main() {
    Key a{1, "x"}, b{1, "x"}, c{2, "x"};
    std::println("{} {} {}", members_equal(a, b), members_equal(a, c), members_hash(a) == members_hash(b));
}
```

```text
true false true
```

## Boost.Describe

Describe needs a `BOOST_DESCRIBE_STRUCT` or `BOOST_DESCRIBE_ENUM` macro next to each type. Reflection needs nothing; delete the macros.

| Boost.Describe | C++26 |
|---|---|
| `describe_members<T, mod_public>` | `nonstatic_data_members_of(^^T, access_context::unprivileged())` |
| `describe_members<T, mod_public \| mod_private \| mod_protected>` | `nonstatic_data_members_of(^^T, access_context::unchecked())` |
| `describe_members<T, mod_static>` | `static_data_members_of(^^T, ctx)` |
| `describe_members<T, mod_function>` | `members_of(^^T, ctx)` filtered with `is_function` and `!is_special_member_function` |
| `describe_bases<T, mod_any_access>` | `bases_of(^^T, access_context::unchecked())` |
| `describe_enumerators<E>` | `enumerators_of(^^E)` |
| `D::name`, `D::pointer` | `identifier_of(m)`, `&[:m:]` or `obj.[:m:]` |

To filter by access yourself, use `is_public(m)`, `is_protected(m)` and `is_private(m)` on reflections from `access_context::unchecked()`.

## Registration patterns

Self-registration through CRTP or static initializers runs at program start, depends on initialization order, and can be dropped by the linker. With reflection, put the types in a namespace and scan it at compile time:

```cpp
#include <meta>
#include <print>
#include <vector>

namespace plugins {
struct alpha { static int run() { return 1; } };
struct beta { static int run() { return 2; } };
}

consteval std::vector<std::meta::info> plugin_types() {
    std::vector<std::meta::info> out;
    for (std::meta::info m : std::meta::members_of(^^plugins, std::meta::access_context::current()))
        if (std::meta::is_class_type(m)) out.push_back(m);
    return out;
}

int main() {
    template for (constexpr auto t : std::define_static_array(plugin_types()))
        std::println("{} {}", std::meta::identifier_of(t), [:t:]::run());
}
```

```text
alpha 1
beta 2
```

Do not rely on the order of namespace members. Sort by name if order matters.

## Type lists and index sequences

Recursive templates over type lists become ordinary loops in `consteval` functions over `template_arguments_of`, ending in `substitute`. See the type list recipe in [recipes.md](recipes.md).

`std::index_sequence` tricks for iterating a tuple become `template for`:

```cpp
#include <print>
#include <string>
#include <tuple>

int main() {
    std::tuple<int, std::string, double> t{1, "two", 3.5};
    template for (const auto& x : t) std::println("{}", x);
}
```

```text
1
two
3.5
```

## Keeping a fallback

Choose the implementation with the preprocessor. `if constexpr` is not enough, because a compiler without reflection cannot parse `^^` or `template for` at all. Include `<version>` to get the library macros.

```cpp
#include <print>
#include <string_view>
#include <type_traits>
#include <version>

#if defined(__cpp_impl_reflection) && defined(__cpp_lib_reflection) && defined(__cpp_expansion_statements)
#include <meta>
#define HAS_REFLECTION 1
#else
#define HAS_REFLECTION 0
#endif

enum class Shape { circle, square };

#if HAS_REFLECTION
template <class E>
    requires std::is_enum_v<E>
constexpr std::string_view enum_name(E value) {
    template for (constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E))) {
        if (value == [:e:]) return std::meta::identifier_of(e);
    }
    return {};
}
#else
constexpr std::string_view enum_name(Shape value) {
    switch (value) {
    case Shape::circle: return "circle";
    case Shape::square: return "square";
    }
    return {};
}
#endif

int main() {
    std::println("{} {}", enum_name(Shape::square), HAS_REFLECTION ? "reflection" : "fallback");
}
```

```text
square reflection
```

Test both paths: build once with reflection enabled and once without.
