I want to turn plain structs into tuples, for example to feed them to code that already works with `std::tuple`.

API:
- `template <class T> using tuple_type_t = ...;`: a `std::tuple` whose element types are the types of the public non-static data members of `T`, in declaration order. A type with no such members gives `std::tuple<>`.
- `template <class T> tuple_type_t<T> to_tuple(const T& obj)`: a tuple holding copies of those members of `obj`.

Static data members, member functions and private members are not part of the tuple. It must work for any class type, without listing the members by hand.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

struct Order {
    int id;
    std::string item;
    double price;
};

class Account {
public:
    Account(long number, std::string owner) : number(number), owner(std::move(owner)) {}

    long number;
    std::string owner;
    static inline int created = 0;
    bool active = true;

    int pin() const { return pin_; }

private:
    int pin_ = 1234;
};

struct Empty {};

static_assert(std::is_same_v<tuple_type_t<Order>, std::tuple<int, std::string, double>>);
static_assert(std::is_same_v<tuple_type_t<Account>, std::tuple<long, std::string, bool>>);
static_assert(std::is_same_v<tuple_type_t<Empty>, std::tuple<>>);
static_assert(std::is_same_v<decltype(to_tuple(Order{})), tuple_type_t<Order>>);

int main() {
    Order o{7, "lamp", 19.5};
    auto t = to_tuple(o);
    o.item = "desk";
    std::println("{} {} {}", std::get<0>(t), std::get<1>(t), std::get<2>(t));

    const Account a(4021, "ann");
    auto [number, owner, active] = to_tuple(a);
    std::println("{} {} {}", number, owner, active);

    std::println("{}", std::tuple_size_v<decltype(to_tuple(Empty{}))>);
}
```

It must print exactly:

```
7 lamp 19.5
4021 ann true
0
```

Write the complete header to `solution.hpp` in the current directory.
