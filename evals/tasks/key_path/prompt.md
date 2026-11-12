I want Swift-style key paths for plain structs: a path written as a string such as `"address.street"` or `"favorites[1].qty"` is resolved at compile time to a chain of member accesses and indexing, and gives get, set and a functional update. A wrong path must be caught at compile time, with a message that says where it went wrong.

Everything goes in `namespace kp`. Paths are string literals passed as template arguments.

Path syntax: a name, followed by zero or more steps, each either `.name` or `[N]`. A name is an ASCII letter or `_` followed by any number of ASCII letters, digits and `_`. `N` is one or more decimal digits. Nothing else is allowed, including spaces, an empty path, a leading `[N]` and an empty step.

Meaning of a path for a root type `Root`, starting from the root object:
- A name (first or after `.`) selects the public non-static data member with that name of the current value. The test structs have no base classes. A type that is not a class, or has no public non-static data member with that name (such as `std::string`), has no such member.
- `[N]` on a C array or a `std::array` selects element `N`. On a `std::vector` it selects `at(N)`, so an index out of range throws `std::out_of_range` at run time. No other type can be indexed.

API:
- `template <class Root, path P> consteval std::string_view kp::check()`, called as `kp::check<Person, "address.city">()`: `ok` when the path is valid for `Root`, otherwise the message for the first problem. The returned view refers to static storage.
  - If the path does not follow the syntax, the message is `malformed path`, whatever else is wrong.
  - Otherwise the steps are checked from left to right and the first failing step gives the message. `<prefix>` is the name of `Root` (the identifier it was declared with, such as `Person`), followed by the path text before the failing step, with a `.` in between when that text is not empty: `Person`, `Person.address`, `Person.favorites[1]`.
  - A name that cannot be selected: `<prefix> has no member '<name>'`.
  - `[N]` on a type that cannot be indexed: `<prefix> cannot be indexed`.
  - `[N]` on a C array or `std::array` with `N` not less than its size: `<prefix> has no element <N>`, with `N` in decimal without leading zeros. `std::vector` indices are never checked here.
- `kp::type<Root, P>`: the type of the value the path selects, without reference. For a valid path only.
- `template <path P, class T> ... kp::get(T& obj)`, called as `kp::get<"address.city">(person)`: returns an lvalue reference to the value the path selects in `obj`. `T` is the root type, possibly `const`; the reference is to `const` exactly when `obj` is `const`.
- `kp::set<P>(obj, value)`: `obj` is a non-`const` lvalue of the root type. Assigns `value`, perfectly forwarded, to the value the path selects in `obj`. Returns nothing.
- `kp::over<P>(obj, fn)`: `obj` is a `const` reference to the root type. Returns a copy of `obj` in which the value the path selects has been replaced by `fn(old)`, where `old` is that value of the copy as a `const` lvalue. `obj` itself is not changed.
- `get`, `set`, `over` and `type` accept only paths for which `check` is `ok`. For other paths they must fail during overload resolution or substitution, so a requires-expression containing such a use is `false`. The test checks this with concepts.

`path` above stands for whatever class type you use to pass string literals as template arguments; name it as you like.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <array>
#include <print>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

struct Address {
    std::string street;
    std::string city;
    int zip;
};

struct Item {
    std::string name;
    int qty;
};

struct Person {
    std::string name;
    int age;
    Address address;
    std::array<Item, 2> favorites;
    std::vector<Address> past;
    int scores[3];
};

class Vault {
public:
    int id = 1;

private:
    int code = 1234;
};

static_assert(std::is_same_v<kp::type<Person, "address.zip">, int>);
static_assert(std::is_same_v<kp::type<Person, "favorites[1]">, Item>);
static_assert(std::is_same_v<kp::type<Person, "past[9].city">, std::string>);
static_assert(std::is_same_v<decltype(kp::get<"scores[0]">(std::declval<const Person&>())), const int&>);
static_assert(std::is_same_v<decltype(kp::get<"address">(std::declval<Person&>())), Address&>);

template <class T>
concept zip_path = requires(T& t) { kp::get<"address.zip">(t); };
template <class T>
concept typo_path = requires(T& t) { kp::get<"address.zpi">(t); };
template <class T>
concept far_score = requires(T& t) { kp::set<"scores[3]">(t, 1); };

std::string upper(std::string s) {
    for (char& c : s)
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    return s;
}

void show(const Person& p) {
    std::println("  {} {} at {}, {} {} | {} x{}, {} x{} | past {} | scores {} {} {}", p.name, p.age, p.address.street,
                 p.address.zip, p.address.city, p.favorites[0].name, p.favorites[0].qty, p.favorites[1].name,
                 p.favorites[1].qty, p.past.size(), p.scores[0], p.scores[1], p.scores[2]);
}

int main() {
    Person ada{"ada", 36, {"main st", "london", 1815}, {{{"tea", 1}, {"cake", 2}}}, {{"old rd", "paris", 75}}, {7, 8, 9}};
    show(ada);

    std::println("city: {}", kp::get<"address.city">(ada));
    std::println("second favorite: {}", kp::get<"favorites[1].name">(ada));
    std::println("last score: {}", kp::get<"scores[2]">(ada));
    std::println("past street: {}", kp::get<"past[0].street">(ada));

    kp::set<"address.zip">(ada, 1852);
    kp::get<"favorites[1].qty">(ada) += 5;
    kp::set<"scores[0]">(ada, 10);
    kp::set<"past[0].city">(ada, std::string("rome"));
    std::println("after set:");
    show(ada);
    std::println("  past city: {}", ada.past[0].city);

    const Person& view = ada;
    Person shouted = kp::over<"address.street">(view, upper);
    Person older = kp::over<"age">(kp::over<"name">(ada, upper), [](int a) { return a + 1; });
    std::println("over results:");
    show(shouted);
    show(older);
    std::println("original:");
    show(view);

    try {
        kp::set<"past[3].zip">(ada, 1);
    } catch (const std::out_of_range&) {
        std::println("past[3] is out of range at run time");
    }

    std::println("checks:");
    std::println("  {}", kp::check<Person, "address.city">());
    std::println("  {}", kp::check<Person, "past[12].zip">());
    std::println("  {}", kp::check<Person, "nmae">());
    std::println("  {}", kp::check<Person, "address.stret">());
    std::println("  {}", kp::check<Person, "age.value">());
    std::println("  {}", kp::check<Person, "name[0]">());
    std::println("  {}", kp::check<Person, "address.city[0]">());
    std::println("  {}", kp::check<Person, "scores[3]">());
    std::println("  {}", kp::check<Person, "favorites[2].qty">());
    std::println("  {}", kp::check<Person, "favorites[1].price">());
    std::println("  {}", kp::check<Person, "favorites.name">());
    std::println("  {}", kp::check<Person, "address..city">());
    std::println("  {}", kp::check<Person, "">());
    std::println("  {}", kp::check<Person, "scores[x]">());
    std::println("  {}", kp::check<Person, "past[1">());
    std::println("  {}", kp::check<Person, "[0]">());
    std::println("  {}", kp::check<Person, "address.">());
    std::println("  {}", kp::check<Vault, "code">());
    std::println("  {}", kp::check<Vault, "id">());

    std::println("zip path: {}", zip_path<Person>);
    std::println("typo path: {}", typo_path<Person>);
    std::println("far score: {}", far_score<Person>);
    std::println("zip path on Address: {}", zip_path<Address>);
}
```

It must print exactly:

```
  ada 36 at main st, 1815 london | tea x1, cake x2 | past 1 | scores 7 8 9
city: london
second favorite: cake
last score: 9
past street: old rd
after set:
  ada 36 at main st, 1852 london | tea x1, cake x7 | past 1 | scores 10 8 9
  past city: rome
over results:
  ada 36 at MAIN ST, 1852 london | tea x1, cake x7 | past 1 | scores 10 8 9
  ADA 37 at main st, 1852 london | tea x1, cake x7 | past 1 | scores 10 8 9
original:
  ada 36 at main st, 1852 london | tea x1, cake x7 | past 1 | scores 10 8 9
past[3] is out of range at run time
checks:
  ok
  ok
  Person has no member 'nmae'
  Person.address has no member 'stret'
  Person.age has no member 'value'
  Person.name cannot be indexed
  Person.address.city cannot be indexed
  Person.scores has no element 3
  Person.favorites has no element 2
  Person.favorites[1] has no member 'price'
  Person.favorites has no member 'name'
  malformed path
  malformed path
  malformed path
  malformed path
  malformed path
  malformed path
  Vault has no member 'code'
  ok
zip path: true
typo path: false
far score: false
zip path on Address: false
```

Write the complete header to `solution.hpp` in the current directory.
