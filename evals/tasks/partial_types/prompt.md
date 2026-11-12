I miss TypeScript's `Partial`, `Pick` and `Omit` and Kotlin's `copy`. I want them for plain C++ structs: each derived type must be a real struct whose members have the original names, and values move between related types by member name.

Everything goes in `namespace ts`. Below, "the members of `T`" means the non-static data members of `T`, in declaration order. All test structs are aggregates with only public members and no base classes.
- `ts::partial<T>`: an aggregate struct with one member per member of `T`, with the same name and order, whose type is `std::optional<M>` where `M` is the type of the original member.
- `ts::pick<T, "a b">`: an aggregate struct with the members of `T` whose names are listed, with the same names and types, in the declaration order of `T` (not the order of the list).
- `ts::omit<T, "a b">`: the same, with the members of `T` whose names are not listed.
- `template <class Dst, class Src> constexpr void ts::assign(Dst& dst, const Src& src)`: for each member of `dst` that `src` has a member with the same name for, in `dst`'s order, copies the value across. When the `src` member is an empty `std::optional`, `dst`'s member is left alone. When the `src` member is an engaged `std::optional` and the `dst` member is not an optional, `dst`'s member gets the contained value. Otherwise `dst`'s member is assigned the `src` member as is. `dst` members with no match are left alone.
- `template <class Dst, class Src> constexpr Dst ts::as(const Src& src)`: a value-initialized `Dst` (`Dst{}`), then `assign(result, src)`.
- `template <class T> constexpr T ts::with(T base, partial<T> changes)`: a copy of `base` with `changes` assigned. The second argument must accept a braced designated initializer list, as in the test, so `T` must be deduced from the first argument only.
- `ts::get<"name">(object)`: a function template whose first template argument is a string literal naming a member and whose function argument is a non-const lvalue of class type. It returns a reference to that member of `object`.
- `ts::problem(std::meta::info type, std::string_view name)`: a `consteval` function returning `std::string_view`: `ok` if `type` has a member called `name`, otherwise `<type> has no member '<name>'`, where `<type>` is the struct name as written in its declaration.
- `template <class T> std::string ts::show(const T& object)`: `{`, then `name: value` for each member, separated by `, `, then `}`. Values are written with `std::format("{}", value)`. A `std::optional` member is written as its contained value when it has one and as `none` when it is empty.

Rules:
- The name list for `pick` and `omit` is a string literal of member names separated by single spaces.
- Naming the same derived type twice with the same arguments gives the same type.
- `ts::get` with a name that the class does not have must not be callable: a requires-expression that calls it is false, as the test checks. A `pick` or `omit` that lists a name the class does not have must not compile, but the test does not do that.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <type_traits>

struct User {
    int id;
    std::string name;
    std::string email;
    std::string password_hash;
};

using UserPatch = ts::partial<User>;
using PublicUser = ts::omit<User, "password_hash">;
using Login = ts::pick<User, "email password_hash">;
using Contact = ts::pick<User, "email id">;
using IdOnly = ts::omit<User, "password_hash name email">;

static_assert(std::is_same_v<decltype(UserPatch::id), std::optional<int>>);
static_assert(std::is_same_v<decltype(UserPatch::name), std::optional<std::string>>);
static_assert(std::is_same_v<ts::partial<User>, UserPatch>);
static_assert(std::is_same_v<ts::pick<User, "email password_hash">, Login>);
static_assert(std::is_aggregate_v<UserPatch> && std::is_aggregate_v<Login>);
static_assert(sizeof(Login) == 2 * sizeof(std::string));
static_assert(sizeof(IdOnly) == sizeof(int));
static_assert(std::is_same_v<decltype(PublicUser::email), std::string>);

template <class T>
concept has_name_key = requires(T& t) { ts::get<"name">(t); };
template <class T>
concept has_nme_key = requires(T& t) { ts::get<"nme">(t); };

int main() {
    User ada{1, "ada", "ada@example.org", "x9f"};
    User renamed = ts::with(ada, {.name = "countess", .email = "al@example.org"});
    std::println("{}", ts::show(renamed));
    std::println("{}", ts::show(ts::as<PublicUser>(renamed)));
    std::println("{}", ts::show(ts::as<Login>(ada)));
    std::println("{}", ts::show(ts::as<Contact>(ada)));
    std::println("{}", ts::show(ts::as<User>(ts::as<Login>(ada))));

    UserPatch patch{.id = 7};
    std::println("{}", ts::show(patch));
    ts::assign(ada, patch);
    ts::get<"name">(ada) += " lovelace";
    std::println("{}", ts::show(ada));

    UserPatch more{.email = "ada@lovelace.org"};
    ts::assign(patch, more);
    std::println("{}", ts::show(patch));
    std::println("{}", ts::show(ts::with(ada, patch)));
    std::println("{}", ts::show(ts::as<UserPatch>(ts::as<Contact>(ada))));

    std::println("get name on User: {}, on Login: {}", has_name_key<User>, has_name_key<Login>);
    std::println("get nme on User: {}", has_nme_key<User>);
    std::println("{}", ts::problem(^^User, "pasword_hash"));
    std::println("{}", ts::problem(^^Login, "email"));
}
```

It must print exactly:

```
{id: 1, name: countess, email: al@example.org, password_hash: x9f}
{id: 1, name: countess, email: al@example.org}
{email: ada@example.org, password_hash: x9f}
{id: 1, email: ada@example.org}
{id: 0, name: , email: ada@example.org, password_hash: x9f}
{id: 7, name: none, email: none, password_hash: none}
{id: 7, name: ada lovelace, email: ada@example.org, password_hash: x9f}
{id: 7, name: none, email: ada@lovelace.org, password_hash: none}
{id: 7, name: ada lovelace, email: ada@lovelace.org, password_hash: x9f}
{id: 7, name: none, email: ada@example.org, password_hash: none}
get name on User: true, on Login: false
get nme on User: false
User has no member 'pasword_hash'
ok
```

Write the complete header to `solution.hpp` in the current directory.
