I keep missing small features from other languages: Swift's `CaseIterable`, Java's `valueOf` and `name`, Pascal sets, Ada range checks, Go struct tags, Python's `repr` and Rust's derived `Hash`. I want one small header that gives me all of them for plain enums and structs, without writing anything per type.

Everything goes in `namespace lang`.

Enums:
- `template <class E> constexpr std::array<E, N> lang::cases`: a variable template holding every enumerator of `E` in declaration order. `N` is the number of enumerators, counting enumerators that share a value separately.
- `template <class E> constexpr std::string_view lang::name(E value)`: the name of the first enumerator, in declaration order, whose value equals `value`. If no enumerator has that value, `"?"`.
- `template <class E> constexpr std::optional<E> lang::parse(std::string_view text)`: the enumerator whose name equals `text` exactly (case sensitive), or `std::nullopt`.
- `template <class E> struct lang::set`: a set of enumerators of `E`, built from a braced list of enumerators such as `lang::set<Permission>{Permission::write, Permission::read}`. The list may be empty and may repeat enumerators. The enumerator values are not bit masks: any values work, as long as they are distinct. Building a set and calling `contains` must work in constant expressions.
  - `constexpr bool contains(E e) const`: whether `e` is in the set.
  - `std::string text() const`: `{`, then the names of the enumerators in the set in declaration order of `E` (not in the order they were given), separated by `, `, then `}`. An empty set gives `{}`.
  - The test only uses sets of enums whose enumerators have distinct values, with at most 64 enumerators.

Struct members:
- `lang::between{low, high}`: an annotation for an integer member, giving the inclusive range of allowed values. `low` and `high` are integers.
- `lang::not_empty`: an annotation object for a member that must not be empty, such as a string or a vector.
- `lang::tag("key", "value")`: an annotation that attaches a free text pair to a member, like a Go struct tag. A member may have several tags.
- `template <class T> std::vector<std::string> lang::violations(const T& object)`: one message per broken rule, for the members of `T` in declaration order. For each member, first its range is checked, then emptiness:
  - a member with `between{low, high}` whose value is below `low` or above `high` gives `<member> = <value> is outside <low>..<high>`, all numbers written as by `std::format("{}")`.
  - a member with `not_empty` for which `std::ranges::empty` is true gives `<member> is empty`.
- `lang::tag_of(std::meta::info member, std::string_view key)`: returns a `std::string_view` with the value of the first tag of `member`, in annotation order, whose key equals `key`. If there is none, an empty view. It must be usable in a constant expression, as in the test.

Structs as values:
- `template <class T> std::string lang::repr(const T& object)`: the name of `T` without namespaces, `(`, then `<member>=<value>` for each member in declaration order separated by `, `, then `)`. A value of enum type is written as `lang::name(value)`. A value whose type is convertible to `std::string_view` is written between single quotes, with no escaping. Any other value is written as by `std::format("{}", value)`.
- `template <class T> std::size_t lang::hash(const T& object)`: a hash made from `std::hash` of every member. Equal values give equal hashes. Every member must take part, so that changing one member changes the hash in practice; the test checks this for one pair.

Rules:
- `T` is always an aggregate whose members are all public non-static data members.
- Members may also carry annotations from other libraries, like `audit::reviewed` in the test. Those must be ignored.
- The annotation spellings must compile exactly as they appear in the test.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <vector>

enum class Weekday { mon, tue, wed, thu, fri, sat, sun };
enum class Permission { read = 4, write = 2, execute = 1 };
enum class Level { low = 1, normal = 2, standard = 2, high = 3 };

namespace audit {
struct reviewed {
    int year;
};
}  // namespace audit

struct Signup {
    [[=lang::not_empty, =lang::tag("form", "user-name"), =lang::tag("json", "user")]] std::string user;
    [[=lang::between{13, 120}]] int age;
    [[=audit::reviewed{2024}, =lang::between{1, 12}, =lang::tag("form", "birth-month")]] int month;
    [[=lang::tag("json", "points"), =lang::not_empty]] std::vector<int> scores;
    Level level;
    bool admin;
};

struct Point {
    int x;
    long y;
    std::string label;
};

int main() {
    for (Weekday day : lang::cases<Weekday>) std::print("{} ", lang::name(day));
    constexpr std::size_t days = lang::cases<Weekday>.size();
    std::println("are {} days", days);
    for (Level level : lang::cases<Level>) std::print("{} ", lang::name(level));
    std::println("are {} levels", lang::cases<Level>.size());
    std::println("first permission: {}", lang::name(lang::cases<Permission>[0]));
    std::println("name of Permission 3: {}", lang::name(static_cast<Permission>(3)));

    constexpr std::optional<Weekday> thu = lang::parse<Weekday>("thu");
    std::println("parse thu: {}", thu.has_value() && *thu == Weekday::thu);
    std::println("parse someday: {}", lang::parse<Weekday>("someday").has_value());
    std::println("parse Read: {}", lang::parse<Permission>("Read").has_value());
    std::println("parse standard is normal: {}", lang::parse<Level>("standard") == Level::normal);

    lang::set<Permission> allowed{Permission::write, Permission::read};
    std::println("{} contains execute: {}", allowed.text(), allowed.contains(Permission::execute));
    constexpr bool reads = lang::set<Permission>{Permission::read}.contains(Permission::read);
    std::println("{{read}} contains read: {}", reads);
    lang::set<Weekday> weekend{Weekday::sun, Weekday::sat, Weekday::sun};
    std::println("weekend: {}", weekend.text());
    lang::set<Weekday> none{};
    std::println("none: {}, contains mon: {}", none.text(), none.contains(Weekday::mon));

    for (const std::string& problem : lang::violations(Signup{"", 9, 14, {}, Level::low, false}))
        std::println("{}", problem);
    std::println("problems with a good signup: {}", lang::violations(Signup{"ada", 36, 12, {1}, Level::high, true}).size());
    std::println("problems at the limits: {}", lang::violations(Signup{"x", 120, 1, {0}, Level::low, false}).size());

    constexpr std::string_view month_form = lang::tag_of(^^Signup::month, "form");
    std::println("month is sent as {}", month_form);
    std::println("user is sent as form [{}], json [{}]", lang::tag_of(^^Signup::user, "form"),
                 lang::tag_of(^^Signup::user, "json"));
    std::println("age has form tag [{}]", lang::tag_of(^^Signup::age, "form"));
    std::println("scores has json tag [{}]", lang::tag_of(^^Signup::scores, "json"));

    Signup ada{"ada", 36, 12, {90, 85}, Level::standard, true};
    std::println("{}", lang::repr(ada));
    std::println("{}", lang::repr(Point{3, -4, "corner"}));

    Point p{1, 2, "a"};
    std::println("equal values, equal hashes: {}", lang::hash(p) == lang::hash(Point{1, 2, "a"}));
    std::println("other label, other hash: {}", lang::hash(p) != lang::hash(Point{1, 2, "b"}));
}
```

It must print exactly:

```
mon tue wed thu fri sat sun are 7 days
low normal normal high are 4 levels
first permission: read
name of Permission 3: ?
parse thu: true
parse someday: false
parse Read: false
parse standard is normal: true
{read, write} contains execute: false
{read} contains read: true
weekend: {sat, sun}
none: {}, contains mon: false
user is empty
age = 9 is outside 13..120
month = 14 is outside 1..12
scores is empty
problems with a good signup: 0
problems at the limits: 0
month is sent as birth-month
user is sent as form [user-name], json [user]
age has form tag []
scores has json tag [points]
Signup(user='ada', age=36, month=12, scores=[90, 85], level=normal, admin=true)
Point(x=3, y=-4, label='corner')
equal values, equal hashes: true
other label, other hash: true
```

Write the complete header to `solution.hpp` in the current directory.
