---
name: cpp26-reflection
description: "Writes, reviews and fixes standard C++26 static reflection code: the meta header, std::meta::info, the ^^ operator, [: :] splices, template for, annotations and define_aggregate. Use when code includes the meta header or uses reflection, when iterating struct members or enumerators, generating enum-to-string, serializers, printers, comparisons or struct-of-arrays, or when replacing magic_enum, Boost.PFR, Boost.Describe, X-macros or hand-written member lists."
---

# C++26 static reflection

C++26 reflection works entirely at compile time. `^^x` produces a `std::meta::info` value describing a type, member, function, enumerator, namespace or value. `consteval` functions in `<meta>` query it. `[: r :]` splices it back into code. `template for` stamps out a copy of its body per element. At run time only the ordinary code you generated is left: there is no runtime metadata.

Much published reflection code predates the standard and uses old syntax and names. Write only the spellings in this skill and in [references/api.md](references/api.md). If existing code uses an unfamiliar spelling, look it up in [references/renames.md](references/renames.md).

## The core loop

Almost every reflection task starts from this shape:

```cpp
#include <meta>
#include <print>

struct Point { int x; int y; };

template <class T>
void print_fields(const T& obj) {
    template for (constexpr auto m : std::define_static_array(
                      std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::current()))) {
        std::println("{}={}", std::meta::identifier_of(m), obj.[:m:]);
    }
}

int main() { print_fields(Point{1, 2}); }
```

```text
x=1
y=2
```

Every part is required:

- `^^T` reflects the type. The operator is two carets.
- Member queries take an access context as a second argument. See [Access](#access).
- Queries return `std::vector<std::meta::info>`, which cannot outlive the constant evaluation that created it. `std::define_static_array` copies it into static storage so `template for` can expand over it. Put the call directly in the loop header.
- The loop variable is `constexpr auto`, so each `m` is a constant that can be spliced.
- `obj.[:m:]` accesses the member. `std::meta::identifier_of(m)` is its name as a `std::string_view` into static storage.

`template for` is not a runtime loop. The body is instantiated once per element, so each iteration can have different types, and `if constexpr` inside it discards branches per element. `break` and `continue` work. It can also expand a brace list `template for (auto x : {a, b, c})` or a tuple-like object.

## Compile time and run time

Every function in `std::meta` is `consteval`. A reflection value cannot reach run time. Follow these rules:

1. Keep reflections in `constexpr` variables, template arguments, `template for` variables, or inside `consteval` functions.
2. Turn results into ordinary values before run time uses them: `constexpr std::size_t n = std::meta::nonstatic_data_members_of(^^T, ctx).size();` then use `n`.
3. Never store a `std::vector<std::meta::info>` in a `constexpr` variable. Use `std::define_static_array`, or keep only scalars and strings.
4. A plain `for` loop over a query result is fine inside a `consteval` function for computing things (counting, searching, building a vector), but its loop variable is not a constant, so it cannot be spliced. Splice only `template for` variables, `constexpr` variables and template arguments.
5. A parameter of a `consteval` function is not a constant expression either. To splice a reflection inside a function, take it as a template parameter: `template <std::meta::info R> auto get(auto& obj) { return obj.[:R:]; }`.
6. Make a non-template function that returns or takes `std::meta::info` `consteval`. In a `constexpr` function template, a `std::meta` call outside a constant context makes that specialization `consteval`, so it can no longer be called at run time. Put such calls in `constexpr` variables, `template for` headers or `if constexpr` conditions.
7. A runtime `if` or `?:` does not stop a `consteval` call in the other branch from being evaluated. Guard with `if constexpr` on a `constexpr bool`, inside a template or `template for` body.
8. If you name the static array before the loop, make it `static constexpr`: `static constexpr auto members = std::define_static_array(...);`. A plain `constexpr` local is not enough: `template for` binds a reference to it, and a reference to a local is not a constant expression.
   The elements of `define_static_array` must be structural too. A vector of `std::string_view`, or of a struct holding a `std::string_view` or `std::string`, does not work: store `const char*` from `std::define_static_string` instead, as in `struct row { const char* name; int n; };`.
9. A class with a `std::meta::info` member, or a span or array of them, is consteval-only too. Even reading its plain fields at run time is ill-formed: copy them first, `constexpr int begin = part.begin;`, then use `begin`.
10. `std::to_string` is not usable at compile time. To build text with numbers in `consteval` code, use `std::to_chars` into a `char` buffer, then append.

## Splices

| Splice | Use |
|---|---|
| `[: r :]` | an expression: a variable, function, enumerator, or reflected value |
| `obj.[: m :]`, `ptr->[: m :]` | a member access |
| `typename [: r :]` | a type. Write `typename` everywhere except where only a type can appear (after `using X =`, in a base-specifier) |
| `template [: r :]<args>` | a template specialization or call |
| `([: r :])` | a value splice used as a template argument, e.g. `std::array<int, ([:r:])>` |

A type splice inside a template argument list needs `typename`: `std::remove_cvref_t<typename [: std::meta::type_of(m) :]>`. A bare `[: ... :]` there does not compile.

`^^` takes a name or a type, not an arbitrary expression: `^^x`, `^^S::m`, `^^int`, `^^std::vector`, `^^ns`, `^^::`. To reflect the value of an expression, use `std::meta::reflect_constant(expr)`.

You cannot reflect an overload set (`^^f` where `f` is overloaded) or a using-declarator. Reflect a specific declaration, or select a member from `members_of` by type or parameters.

## Access

Member queries need a `std::meta::access_context`:

| Context | Sees |
|---|---|
| `access_context::current()` | what is accessible where `current()` is written |
| `access_context::unprivileged()` | public members only |
| `access_context::unchecked()` | everything, including private |

The queries that take it: `members_of`, `nonstatic_data_members_of`, `static_data_members_of`, `bases_of`, `subobjects_of`, `has_inaccessible_*` and `is_accessible`. `enumerators_of` does not take one.

`current()` is evaluated where it is written. Inside a library helper it sees only what the helper can access, which for a helper at namespace scope means public members. To give a helper the caller's access, take the context as a defaulted parameter, because default arguments are evaluated at the call site:

```cpp
#include <meta>

consteval std::size_t field_count(std::meta::info type,
                                  std::meta::access_context ctx = std::meta::access_context::current()) {
    return std::meta::nonstatic_data_members_of(type, ctx).size();
}

struct S { int a; int b; };
static_assert(field_count(^^S) == 2);

int main() {}
```

Use `unchecked()` deliberately, for example in a serializer that must see private members, and say so in a comment. A splice is not access-checked, so `obj.[:m:]` works for any `m` you obtained.

## Queries and their traps

- `members_of` returns everything: data members, member functions, nested types, static members, and the implicitly declared constructors, destructor and assignment operators. For data, use `nonstatic_data_members_of`. For functions, filter with `is_function(m)` and exclude `is_special_member_function(m)`, `is_constructor(m)`, `is_destructor(m)` and `is_operator_function(m)` as needed.
- Class members, bases and enumerators come in declaration order. Namespace members from `members_of(^^ns, ctx)` come in no fixed order; sort them, for example by `identifier_of`, when order matters.
- `identifier_of(r)` fails at compile time unless `has_identifier(r)`. Fundamental types (`^^int`), cv-qualified types, template specializations (`^^std::vector<int>`), constructors, operators and unnamed entities have no identifier.
- `display_string_of(r)` text is implementation-defined. Use it only for human-readable messages, never for keys, tests or serialization.
- `type_of(r)` takes an entity with a type (variable, data member, function, parameter, enumerator, base, annotation, value). It does not take a type.
- `^^Alias` reflects the alias, not the aliased type: `^^MyInt != ^^int`. Standard names such as `std::string` and `std::size_t` are aliases too, so `type_of(m) == ^^std::string` is false for a `std::string` member. Use `dealias(r)` on both sides, or `is_same_type`, before comparing types or calling `template_arguments_of`.
- `^^` cannot reflect a name introduced by a using-declaration. The standard library may declare C names such as `std::uint32_t` or `std::int8_t` that way, so `^^std::uint32_t` is not portable. Go through a template instead: `template <class T> constexpr std::meta::info type = ^^T;` then `type<std::uint32_t>`.
- Compare reflections with `==` and `!=`. They have no `<`. To order types, use `std::meta::type_order(a, b)`.
- `^^T && x` reflects the type `T&&`, so `r == ^^T && ok` does not compile. Write `(^^T)` when `&&` or `&` follows.
- A class is incomplete inside its own definition, so member queries on it fail there, for example in a `static_assert` in the class body. Query from outside the class, or from a member function body, where the class is complete.
- `bases_of(^^T, ctx)` returns base relationships. Use `type_of(b)` for the base type. To reach the base subobject, cast: `using Base = [:std::meta::type_of(b):]; const Base& base = obj;`. The splice `obj.[:b:]` is also standard.
- Type properties use names ending in `_type`: `is_class_type`, `is_enum_type`, `is_same_type`, `remove_cvref`, `is_integral_type`. Never `type_is_class` or `test_trait`.
- To build a type from a template and arguments, use `substitute(^^std::vector, {^^int})` and splice it with `typename [: ... :]`. `template_arguments_of(r)` and `template_of(r)` go the other way.

## Values

- `std::meta::reflect_constant(v)` reflects a value. `v` must be a structural type (scalars, and classes with only public structural members and no `std::string`).
- `std::meta::extract<T>(r)` gets the value back. `T` must match the reflected type exactly, apart from cv-qualifiers.
- `[: r :]` on a value reflection also produces the value.
- `std::meta::reflect_object(obj)` and `std::meta::reflect_function(f)` reflect a specific object or function.
- `std::define_static_string(sv)` turns a compile-time string into a `const char*` into static storage. Use it to carry strings in structural values and annotations.

## Annotations

An annotation is a constant attached to a declaration: `[[=value]] int id;`.

- It goes in its own `[[ ]]`, never mixed with attributes: `[[nodiscard]] [[=tag{}]] int f();`.
- On an enumerator it goes after the name: `enum class E { a [[=1]], b };`.
- The value must be structural. `std::string_view`, `std::string` and pointers into string literals do not work. For text, store a `const char*` from `std::define_static_string` or a fixed `char` array.
- Read them with `annotations_of(r)` and `std::meta::extract<A>(a)`. The type of a class-type annotation is `const A`, so compare types with `std::meta::remove_const(std::meta::type_of(a)) == ^^A`, or use `annotations_of_with_type(r, ^^A)`.
- Put annotation types in a namespace. A global `rename` or `remove` collides with the C library functions of that name.

```cpp
#include <meta>
#include <string_view>

namespace json {
struct rename {
    const char* name;
    consteval rename(std::string_view n) : name(std::define_static_string(n)) {}
};
struct skip_t {};
inline constexpr skip_t skip{};
}

template <class A>
consteval bool has_annotation(std::meta::info r) {
    for (std::meta::info a : std::meta::annotations_of(r))
        if (std::meta::remove_const(std::meta::type_of(a)) == ^^A) return true;
    return false;
}

struct User {
    [[=json::rename("user_id")]] int id;
    [[=json::skip]] int secret;
};

static_assert(has_annotation<json::skip_t>(^^User::secret));
static_assert(!has_annotation<json::skip_t>(^^User::id));
constexpr auto id_name = std::meta::extract<json::rename>(std::meta::annotations_of(^^User::id)[0]);
static_assert(std::string_view(id_name.name) == "user_id");

int main() {}
```

## Generating types

`std::meta::define_aggregate(class_reflection, specs)` gives an incomplete class its data members. Rules:

- Call it inside a `consteval { }` block, at namespace or class scope.
- The class must be declared but not yet defined.
- Give each member spec a name: `std::meta::data_member_spec(^^int, {.name = "x"})`. Options also include `.alignment`, `.bit_width` and `.no_unique_address`. Only a bit-field may leave out the name, `{.bit_width = 4}`, which makes padding bits. Unnamed bit-fields are not listed by `nonstatic_data_members_of`.
- It only adds data members. C++26 cannot generate member functions, enumerators or arbitrary declarations.

For a type generated from a template parameter, declare a nested class in a class template and define it in a `consteval` block inside that template. See the struct-of-arrays recipe in [references/recipes.md](references/recipes.md).

## Errors

A `std::meta` function that cannot do what was asked throws `std::meta::exception` during constant evaluation. Uncaught, it becomes a compile error, usually with the `what()` text in the diagnostic. You can catch it inside a `consteval` function:

```cpp
#include <meta>

consteval bool has_type(std::meta::info r) {
    try {
        (void)std::meta::type_of(r);
        return true;
    } catch (const std::meta::exception&) {
        return false;
    }
}
```

Prefer checking preconditions instead (`has_identifier`, `is_type`, `has_template_arguments`).

## Not in C++26

These appear in older papers, blog posts and experimental compilers. They are not standard; do not use them: `std::meta::expand`, `reflect_invoke`, `define_class`, `define_enum`, `reflexpr`, attribute reflection (`attributes_of`), token injection, and splices used directly as template arguments without parentheses. There is no runtime reflection; to look something up by a runtime string, generate the comparison code with `template for` (see [references/recipes.md](references/recipes.md)).

## Portability

Reflection is new. A compiler may not support it, may need an option to enable it, or may implement an older draft. Do not guess flags: check the compiler's documentation, and detect support with the standard feature-test macros:

| Macro | Feature |
|---|---|
| `__cpp_impl_reflection` | `^^`, splices, `std::meta::info` |
| `__cpp_lib_reflection` | the `<meta>` library |
| `__cpp_expansion_statements` | `template for` |
| `__cpp_lib_define_static` | `std::define_static_array` and friends |
| `__cpp_constexpr_exceptions` | catching `std::meta::exception` |

For code that must also build without reflection, select the implementation with the preprocessor, not with `if constexpr`, because an older compiler cannot parse `^^` or `template for` at all. See [references/migration.md](references/migration.md) for a complete fallback example and for when a pre-C++26 library is still the better choice.

## Before you finish

1. Compile and run the code if a reflection-capable compiler is available.
2. Check every `std::meta` name against [references/api.md](references/api.md), and translate old names with [references/renames.md](references/renames.md).
3. Check that every member query has an access context and that the context is the one you intend.
4. Check that no reflection value or `std::vector<std::meta::info>` is used at run time.
5. Check `typename` on type splices and `template` on template splices.

## References

- [references/recipes.md](references/recipes.md): complete, compiled programs: enum to and from string, struct printing with nested structs and bases, field-name arrays, finding a member by name, setting a field by a runtime name, serialization with annotations, struct of arrays, function parameters, transforming type lists.
- [references/api.md](references/api.md): every declaration in `<meta>`, grouped by purpose.
- [references/renames.md](references/renames.md): old spellings and their current equivalents.
- [references/migration.md](references/migration.md): replacing X-macros, magic_enum, Boost.PFR, Boost.Describe and registration patterns; when not to use reflection; feature-tested fallbacks.
