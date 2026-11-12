I want TypeScript declarations for the types my C++ server sends as JSON, generated from the C++ types so the two never disagree. Starting from one root type, the generator should follow every type it uses, emit each enum and struct once, and put each declaration after the declarations it depends on.

Everything goes in `namespace ts`:
- `template <class T> std::string ts::emit()`: the declarations of every enum and class type reachable from `T` (including `T`, if it is one), in the order described below. Each declaration ends with a newline, and consecutive declarations are separated by one empty line.
- `template <class T> std::string ts::type_expr()`: the TypeScript type expression for `T`.

Type expressions. `const` and `volatile` are ignored, and aliases such as `shop::Cents` or `std::int64_t` stand for the type they name. `<X>` below means the expression for `X`.
- `bool`: `boolean`
- an integral type of 8 bytes: `bigint`
- any other arithmetic type, including `char`: `number`
- `std::string`, `std::string_view`: `string`
- `std::monostate`: `null`
- `std::optional<X>`: `<X> | null`
- `std::vector<X>`, `std::array<X, N>`: `<X>[]`, but `(<X>)[]` when `X` is a `std::optional` or a `std::variant` with more than one alternative
- `std::map<std::string, V>`: `Record<string, <V>>`
- `std::variant<A, B, ...>`: the expressions of the alternatives joined with ` | `
- `std::tuple<A, B, ...>`, `std::pair<A, B>`: `[<A>, <B>, ...]`, the elements joined with `, `
- any other enum or class type: its unqualified name, even when it is nested in a class or namespace.
- The test uses no other types.

Declarations:
- An enum: `export type <Name> = "<e1>" | "<e2>" | ...;`, with the enumerator names in declaration order.
- A class: the line `export interface <Name> {`, or `export interface <Name> extends <B1>, <B2> {` when the class has public direct base classes (in declaration order), then one line per public non-static data member in declaration order, then the line `}`. Members of base classes are not repeated. A member line is two spaces followed by `<member>: <type expression>;`, except that a member of type `std::optional<X>` is written `<member>?: <X>;`.

Order. `emit<T>()` visits `T`, and visiting a type does this:
- an arithmetic type, a string type or `std::monostate`: nothing.
- `std::optional`, `std::vector`, `std::array`, `std::variant`, `std::tuple` and `std::pair`: visit each type argument in order (only the element type of `std::array`). `std::map`: visit the value type.
- an enum or class type that has been visited before, including one whose visit has not finished yet: nothing.
- any other enum: emit its declaration.
- any other class: visit its public direct base classes in declaration order, then the types of its public non-static data members in declaration order, then emit its declaration.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <variant>
#include <vector>

namespace shop {

enum class Currency { usd, eur, jpy };

using Cents = std::int64_t;

struct Money {
    Cents amount;
    Currency currency;
};

struct Entity {
    std::uint32_t id;
};

struct Audited {
    std::string created_by;
};

struct Address {
    std::string street;
    std::optional<std::string> zip;
};

struct Customer : Entity {
    std::string name;
    std::vector<Address> addresses;
    std::map<std::string, std::string> tags;
    bool vip;
};

struct Item {
    std::string sku;
    const unsigned short quantity;
    Money price;
};

struct Card {
    std::string last4;
};

struct Transfer {
    std::string iban;
    Money fee;
};

struct Order : Entity, Audited {
    enum Status { open, paid, shipped };

    Customer customer;
    std::vector<Item> items;
    std::variant<std::monostate, Card, Transfer> payment;
    std::vector<std::optional<double>> discounts;
    Status status;
    std::pair<double, double> location;
    std::array<float, 3> box;
    std::optional<Money> refund;
    static constexpr int version = 2;

private:
    int secret = 0;
};

}  // namespace shop

// a self-referential type and two types that refer to each other
struct Comment {
    std::string text;
    std::vector<Comment> replies;
};

struct Team;

struct Person {
    std::string name;
    std::optional<std::vector<Team>> teams;
};

struct Team {
    std::string title;
    std::vector<Person> members;
    std::map<std::string, Team> subteams;
};

int main() {
    std::print("{}", ts::emit<shop::Order>());
    std::println("---");
    std::print("{}", ts::emit<Comment>());
    std::println("---");
    std::print("{}", ts::emit<Team>());
    std::println("---");
    std::print("{}", ts::emit<shop::Currency>());
    std::println("---");
    std::println("{}", ts::type_expr<std::map<std::string, std::vector<std::optional<int>>>>());
    std::println("{}", ts::type_expr<std::variant<char, std::string_view, std::vector<shop::Card>>>());
    std::println("{}", ts::type_expr<std::tuple<bool, shop::Cents, shop::Currency>>());
    std::println("{}", ts::type_expr<std::optional<std::variant<int, bool>>>());
    std::println("{}", ts::type_expr<std::vector<std::variant<std::monostate, long long>>>());
    std::println("{}", ts::type_expr<std::vector<std::variant<shop::Money>>>());
    std::println("{}", ts::type_expr<std::array<std::vector<std::pair<int, std::string>>, 2>>());
}
```

It must print exactly:

```
export interface Entity {
  id: number;
}

export interface Audited {
  created_by: string;
}

export interface Address {
  street: string;
  zip?: string;
}

export interface Customer extends Entity {
  name: string;
  addresses: Address[];
  tags: Record<string, string>;
  vip: boolean;
}

export type Currency = "usd" | "eur" | "jpy";

export interface Money {
  amount: bigint;
  currency: Currency;
}

export interface Item {
  sku: string;
  quantity: number;
  price: Money;
}

export interface Card {
  last4: string;
}

export interface Transfer {
  iban: string;
  fee: Money;
}

export type Status = "open" | "paid" | "shipped";

export interface Order extends Entity, Audited {
  customer: Customer;
  items: Item[];
  payment: null | Card | Transfer;
  discounts: (number | null)[];
  status: Status;
  location: [number, number];
  box: number[];
  refund?: Money;
}
---
export interface Comment {
  text: string;
  replies: Comment[];
}
---
export interface Person {
  name: string;
  teams?: Team[];
}

export interface Team {
  title: string;
  members: Person[];
  subteams: Record<string, Team>;
}
---
export type Currency = "usd" | "eur" | "jpy";
---
Record<string, (number | null)[]>
number | string | Card[]
[boolean, bigint, Currency]
number | boolean | null
(null | bigint)[]
Money[]
[number, string][][]
```

Write the complete header to `solution.hpp` in the current directory.
