Our stored records change shape between releases: members get renamed, added, dropped or turn into nested structs of a newer version. I want one generic function that converts a value of an old version into a newer one by matching members by name, with annotations for renamed members and the newer struct's default member initializers for members that have no counterpart.

Everything goes in `namespace ver`. All structs involved are aggregates whose members are all public.

API:
- `ver::from`: an annotation type, used as `[[=ver::from("old_name")]]` on a member of the destination struct. It says the member may take its value from a source member called `old_name`. A member may carry several `from` annotations.
- `template <class To, class From> consteval std::string_view check()`: returns `"ok"` when `From` converts to `To`, and otherwise a message describing the first problem (see below). The returned view stays valid at run time.
- `template <class To, class From> constexpr To convert(const From& from)`: returns `from` converted to `To`. It can be used in constant expressions when the member types allow it. When `check<To, From>()` is not `"ok"`, `convert<To>(from)` must not compile, and must fail as part of overload resolution, so that a requires-expression containing the call is `false`. The test checks this with a concept.

Finding the source of each member `d` of `To`:
- The candidate names are `d`'s own name first, then the names of its `from` annotations in the order they are written.
- The source is the non-static data member of `From` whose name is the first candidate name that `From` has. The order of the members in `From` does not matter.
- If no candidate name is a member of `From`, `d` has no source.
- Members of `From` that are the source of nothing are ignored.

The result:
- It starts as `To{}`. Members of `To` without a source keep that value, which comes from their default member initializer.
- Each member of `To` with a source gets the converted value of that source member, by these rules on the destination member type `D` and the source member type `S`:
  1. If `const S&` is implicitly convertible to `D`, the value is converted that way (for example `int` to `long long`, `int` to `std::optional<int>`, or a copy when the types are equal).
  2. Otherwise, if `D` and `S` are both aggregate class types, the value is `convert<D>` of the source member, with all the same rules applied recursively.
  3. Otherwise the conversion is not possible.

Problems, reported by `check`:
- A member of `To` that has no source and no default member initializer: `no source for '<path>'`.
- A member of `To` whose source cannot be converted by the rules above: `cannot convert '<path>'`.
- `<path>` is the member's name. For a member found while checking a nested conversion under rule 2, it is the names from the outer member down to that member, joined with `.`, as in `address.zip`.
- The members of `To` are checked in declaration order, and a member converted under rule 2 is checked completely, depth first, before the next member of its struct. `check` reports only the first problem met in that order.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <optional>
#include <print>
#include <string>
#include <vector>

struct AddressV1 {
    std::string street;
    std::string city;
};

struct UserV1 {
    int id;
    std::string name;
    std::string mail;
    AddressV1 address;
    int age;
    bool legacy;
};

struct AddressV2 {
    std::string street;
    [[=ver::from("city")]] std::string town;
    std::string country = "NL";
};

struct UserV2 {
    long long id;
    [[=ver::from("name")]] std::string full_name;
    [[=ver::from("mail")]] std::string email;
    AddressV2 address;
    std::optional<int> age;
    std::vector<std::string> tags{"new"};
    double score = 1.5;
};

// v3 can be made from v1 or from v2
struct UserV3 {
    long long id;
    [[=ver::from("full_name")]] [[=ver::from("name")]] std::string display_name;
    [[=ver::from("email")]] [[=ver::from("mail")]] std::string contact;
    AddressV2 address;
    std::vector<std::string> tags{"v3"};
};

// has both a full_name and a name, and its own display_name
struct Badge {
    long long id;
    std::string display_name;
    std::string full_name;
    std::string name;
    std::string mail;
    AddressV1 address;
};

// has a name before its full_name, and both mail and email
struct Profile {
    long long id;
    std::string name;
    std::string full_name;
    std::string mail;
    std::string email;
    AddressV2 address;
    std::vector<std::string> tags;
};

struct Point {
    int x;
    int y;
};

struct Vec3 {
    [[=ver::from("x")]] double a;
    [[=ver::from("y")]] double b;
    double c = 9;
};

static_assert(ver::convert<Vec3>(Point{1, 2}).b == 2);
static_assert(ver::convert<Vec3>(Point{1, 2}).c == 9);

struct NeedsPhone {
    long long id;
    std::string phone;
};

struct AddressZip {
    std::string street;
    std::string zip;
};

struct NeedsZip {
    int id;
    AddressZip address;
};

struct NumericName {
    int name;
};

struct ListAddress {
    std::vector<std::string> address;
};

template <class To, class From>
concept convertible = requires(const From& from) { ver::convert<To>(from); };

void print(const UserV2& u) {
    std::println("v2 id={} full_name={} email={} address={}/{}/{} age={} tags={} score={}", u.id, u.full_name,
                 u.email, u.address.street, u.address.town, u.address.country,
                 u.age ? std::to_string(*u.age) : "none", u.tags, u.score);
}

void print(const UserV3& u) {
    std::println("v3 id={} display_name={} contact={} address={}/{}/{} tags={}", u.id, u.display_name, u.contact,
                 u.address.street, u.address.town, u.address.country, u.tags);
}

int main() {
    UserV1 v1{7, "Ada Lovelace", "ada@example.org", {"1 Main St", "London"}, 36, true};

    UserV2 v2 = ver::convert<UserV2>(v1);
    print(v2);
    v2.tags.push_back("vip");
    v2.address.country = "UK";
    v2.age.reset();
    print(ver::convert<UserV2>(v2));

    print(ver::convert<UserV3>(v1));
    print(ver::convert<UserV3>(v2));

    Badge badge{3, "ada", "Augusta Ada King", "Ada", "ada@badge.org", {"2 Side St", "Paris"}};
    print(ver::convert<UserV3>(badge));
    Profile profile{4, "Ada", "Ada King", "old@example.org", "new@example.org", {"3 High St", "Rome", "IT"}, {}};
    print(ver::convert<UserV3>(profile));

    Vec3 v = ver::convert<Vec3>(Point{4, -5});
    std::println("vec3 {} {} {}", v.a, v.b, v.c);

    std::println("UserV2 from UserV1: {}", ver::check<UserV2, UserV1>());
    std::println("UserV1 from UserV2: {}", ver::check<UserV1, UserV2>());
    std::println("NeedsPhone from UserV1: {}", ver::check<NeedsPhone, UserV1>());
    std::println("NeedsZip from UserV1: {}", ver::check<NeedsZip, UserV1>());
    std::println("NumericName from UserV1: {}", ver::check<NumericName, UserV1>());
    std::println("ListAddress from UserV1: {}", ver::check<ListAddress, UserV1>());
    std::println("Point from Vec3: {}", ver::check<Point, Vec3>());

    std::println("convertible UserV3 <- Badge: {}", convertible<UserV3, Badge>);
    std::println("convertible UserV1 <- UserV2: {}", convertible<UserV1, UserV2>);
    std::println("convertible NeedsZip <- UserV1: {}", convertible<NeedsZip, UserV1>);
    std::println("convertible NumericName <- UserV1: {}", convertible<NumericName, UserV1>);
}
```

It must print exactly:

```
v2 id=7 full_name=Ada Lovelace email=ada@example.org address=1 Main St/London/NL age=36 tags=["new"] score=1.5
v2 id=7 full_name=Ada Lovelace email=ada@example.org address=1 Main St/London/UK age=none tags=["new", "vip"] score=1.5
v3 id=7 display_name=Ada Lovelace contact=ada@example.org address=1 Main St/London/NL tags=["v3"]
v3 id=7 display_name=Ada Lovelace contact=ada@example.org address=1 Main St/London/UK tags=["new", "vip"]
v3 id=3 display_name=ada contact=ada@badge.org address=2 Side St/Paris/NL tags=["v3"]
v3 id=4 display_name=Ada King contact=new@example.org address=3 High St/Rome/IT tags=[]
vec3 4 -5 9
UserV2 from UserV1: ok
UserV1 from UserV2: no source for 'name'
NeedsPhone from UserV1: no source for 'phone'
NeedsZip from UserV1: no source for 'address.zip'
NumericName from UserV1: cannot convert 'name'
ListAddress from UserV1: cannot convert 'address'
Point from Vec3: no source for 'x'
convertible UserV3 <- Badge: true
convertible UserV1 <- UserV2: false
convertible NeedsZip <- UserV1: false
convertible NumericName <- UserV1: false
```

Write the complete header to `solution.hpp` in the current directory.
