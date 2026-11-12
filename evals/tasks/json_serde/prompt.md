I want a JSON writer for plain classes. The user writes the class and a few annotations; there are no macros, no member lists to keep in sync and no registration calls. Classes can also opt in to having their private members written.

Everything goes in `namespace json`:
- `json::skip`: annotation that leaves a member out, used as `[[=json::skip]]`.
- `json::name("key")`: annotation that writes a member under the given key, used as `[[=json::name("role")]]`.
- `json::since(n)`: annotation that writes a member only when the format version is at least `n`, used as `[[=json::since(2)]]`.
- `json::naming`: an enum with enumerators `as_written` and `camel`, used on a class as `[[=json::naming::camel]]` to choose how its member names become keys.
- `json::access`: a class type that a class befriends with `friend struct json::access;` to let the library write its private and protected members.
- `template <class T> std::string json::to_string(const T& value, int version = 1)`: the JSON text of `value` for that format version.
- `json::check(std::meta::info type)`: a `consteval` function returning `std::string_view`, `ok` if the class can be written and otherwise the reason.

The annotation spellings must compile exactly as they appear in the test.

How a value of type `T` is written, first matching rule wins:
1. `bool`: `true` or `false`.
2. other arithmetic types: `std::format("{}", value)`.
3. enums: the identifier of the enumerator equal to the value, in double quotes. The test only uses values that have an enumerator.
4. types convertible to `std::string_view` (here `std::string` and `const char*`): the text in double quotes. The test strings need no escaping, so write them as they are.
5. `std::optional<U>`: `null` if empty, otherwise the contained value.
6. ranges (here `std::vector`): `[`, the elements separated by `,`, `]`.
7. class types that declare a member function named `to_json`: call `value.to_json(out)`, where `out` is the `std::string` being built; it appends the JSON for the value.
8. other classes: `{`, the members separated by `,`, `}`. Each member is `"key":value`.

There is no whitespace anywhere in the output. The format version passes down unchanged to nested values, including elements of ranges and optionals.

Members of a class (rule 8):
- The members written are the non-static data members, in declaration order. A class that befriends `json::access` also has its private and protected members written. All test classes are without base classes.
- A member with `json::skip` is never written and plays no part in key checks.
- A member with `json::since(n)` is written only when the version is at least `n`. Members without it are always written.
- The key of a member is its `json::name` text when it has one, whatever the class naming. Otherwise, if the class carries `json::naming::camel`, it is the member name with each `_` removed and the letter right after it made uppercase (`home_address` becomes `homeAddress`). Otherwise the key is the member name as written. A class's naming applies only to its own members, not to the classes of its members.
- Annotations from other libraries are ignored.

`check(^^T)` for a class `T`:
- If `T` has a non-static data member that the library cannot see, because it is private or protected and `T` does not befriend `json::access`, it returns `<T> has members the library cannot see; add: friend struct json::access;`.
- Otherwise, going through the members that are not skipped in declaration order, if a member's key equals the key of an earlier member, it returns `<T>: two members map to the key '<key>'` for the first such member.
- Otherwise it returns `ok`.

`<T>` is the class name as written in its declaration, without namespaces. `to_string` of a class that `check` rejects must not compile, but the test does not do that.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <format>
#include <optional>
#include <print>
#include <string>
#include <vector>

enum class Role { admin, guest };

struct Money {  // brings its own format
    long cents;
    void to_json(std::string& out) const { out += std::format("\"{}.{:02}\"", cents / 100, cents % 100); }
};

struct [[=json::naming::camel]] Address {
    std::string street_name;
    int house_number;
};

struct Tag {  // no class annotation, so keys are written as declared
    std::string tag_name;
    [[=json::since(2)]] double weight;
};

class [[=json::naming::camel]] Account {
    friend struct json::access;
    int account_id = 7;
    std::string owner_name = "ada";
    [[=json::skip]] std::string password = "swordfish";
    [[=json::name("role")]] Role access_role = Role::admin;
    Money balance{12050};
    std::optional<Address> home_address = Address{"Main St", 3};
    std::optional<std::string> nick_name;
    std::vector<int> recent_logins{3, 5, 8};
    std::vector<Tag> tags{{"vip", 1.5}, {"beta", 0.25}};
    [[=json::since(2)]] bool two_factor = true;
};

struct Clash {
    int user_id;
    [[=json::name("user_id")]] int id;
};

struct [[=json::naming::camel]] CamelClash {
    int item_count;
    [[=json::skip]] int itemCount;
    int itemCount2;
    [[=json::name("item_count2")]] int other;
    int item_count2;
};

class Hidden {
    int secret = 1;

public:
    int shown = 2;
};

struct [[=json::naming::as_written]] Point {
    int x_pos;
    int y_pos;
    const char* label;
};

int main() {
    Account account;
    std::println("{}", json::to_string(account));
    std::println("{}", json::to_string(account, 2));
    std::println("{}", json::to_string(std::vector<Role>{Role::guest, Role::admin}));
    std::println("{}", json::to_string(std::vector<std::optional<Point>>{Point{1, -2, "a"}, std::nullopt}));
    std::println("{}", json::to_string(Money{5}));
    std::println("{}", json::check(^^Account));
    std::println("{}", json::check(^^Clash));
    std::println("{}", json::check(^^CamelClash));
    std::println("{}", json::check(^^Hidden));
}
```

It must print exactly:

```
{"accountId":7,"ownerName":"ada","role":"admin","balance":"120.50","homeAddress":{"streetName":"Main St","houseNumber":3},"nickName":null,"recentLogins":[3,5,8],"tags":[{"tag_name":"vip"},{"tag_name":"beta"}]}
{"accountId":7,"ownerName":"ada","role":"admin","balance":"120.50","homeAddress":{"streetName":"Main St","houseNumber":3},"nickName":null,"recentLogins":[3,5,8],"tags":[{"tag_name":"vip","weight":1.5},{"tag_name":"beta","weight":0.25}],"twoFactor":true}
["guest","admin"]
[{"x_pos":1,"y_pos":-2,"label":"a"},null]
"0.05"
ok
Clash: two members map to the key 'user_id'
CamelClash: two members map to the key 'itemCount2'
Hidden has members the library cannot see; add: friend struct json::access;
```

Write the complete header to `solution.hpp` in the current directory.
