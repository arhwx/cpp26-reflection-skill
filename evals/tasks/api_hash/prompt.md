I want an API snapshot for a namespace: a text that lists the names and signatures of everything public in it, and a hash of that text that I can pin with `static_assert`, so that an accidental API change breaks the build. Changes that do not touch the public API (declaration order, parameter names, function bodies, private members, `detail` namespaces, spelling a type through an alias) must not change the snapshot.

API, all in `namespace surface`:
- `template <std::meta::info NS> constexpr std::string_view snapshot`: a variable template holding the snapshot text of the namespace `NS` (used as `surface::snapshot<^^shop>`), usable in constant expressions.
- `template <std::meta::info NS> constexpr std::uint64_t hash`: a variable template holding the 64-bit FNV-1a hash of `snapshot<NS>`, usable in constant expressions.

Snapshot text: collect the lines described below for every member of `NS`, sort them in ascending order comparing bytes (as `std::string` `<` does), keep duplicates, and write each line followed by `\n`.

Names: an entity's name is written relative to `NS`: the names of the namespaces and classes that enclose it inside `NS`, then its own name, joined with `::`, like `tax::rate` or `Cart::State`. The test refers only to types declared inside `NS`.

Members of a namespace:
- A nested namespace contributes the lines of its own members, with names still relative to `NS`. A namespace named `detail`, at any depth, contributes nothing. Namespaces get no line of their own.
- A type alias: `using <name> = <type>`.
- A template of any kind: `template <name>`.
- A scoped enumeration (the test has no unscoped ones): `enum <name>: <underlying type>`, and one line per enumerator: `enumerator <enum name>::<enumerator name> = <value>`, the value in decimal.
- A class or struct (both written `class`; the test has no unions): `class <name>`, plus the lines of its members as described below.
- A variable: `var <name>: <type>`.
- A function, one line per overload: `fn <name><signature>`. Deleted functions are left out.

Members of a class: only public members count; private and protected ones never contribute.
- A non-static data member: `field <name>: <type>`, where `<name>` includes the class, like `Money::cents`.
- A static data member: `var <name>: <type>`.
- A nested class, enumeration, type alias or template: as in a namespace.
- Member functions count only when user-declared and not deleted; implicitly declared special members and destructors are always left out.
  - A constructor: `ctor <class name>(<parameters>)`, followed by ` noexcept` when it is noexcept.
  - A static member function: `fn <name><signature>`.
  - Any other member function: `method <name><signature>`.
  - The name of an operator function is `operator` followed by its symbol, like `Money::operator==`.

Signature: `(<parameters>)`, then ` const` for a const-qualified member function, then ` noexcept` for a noexcept function, then ` -> <return type>`. Parameters are their types separated by `, `; a parameter with a default argument gets ` = ?` after its type. Parameter names are not part of it.

Types:
- `T&` is written `<T>&`, `T&&` is `<T>&&`, a const `T` is `<T> const`, and a pointer to `T` is `<T>*`. So `const char*` is `char const*`, and `char* const*` is `char* const*`.
- Aliases are replaced by the type they name. On our platform `std::size_t` is `unsigned long`.
- Fundamental types are written `void`, `bool`, `char`, `signed char`, `unsigned char`, `short`, `unsigned short`, `int`, `unsigned int`, `long`, `unsigned long`, `long long`, `unsigned long long`, `float`, `double`, `long double`.
- A class or enumeration type is written as its name, relative to `NS` as above.

Hash: 64-bit FNV-1a over the bytes of the snapshot text: start with `0xcbf29ce484222325`; for each byte, xor the byte (as an unsigned value from 0 to 255) into the hash, then multiply by `0x100000001b3` modulo 2^64.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <cstddef>
#include <print>

namespace shop {
using id_t = unsigned long;
inline constexpr int version = 3;

enum class Currency : unsigned char { eur = 1, usd = 2, gbp = 4 };

struct Money {
    long long cents;
    Currency currency;

    bool operator==(const Money& other) const;
    Money scaled(double factor) const noexcept;
    static Money zero(Currency c = Currency::eur);
};

class Cart {
public:
    enum class State { open, paid = 10 };

    Cart();
    explicit Cart(id_t owner);
    Cart(const Cart&) = delete;
    ~Cart();

    void add(id_t item, int count = 1);
    Money total() const;
    State state() const noexcept;
    const char* label() const;
    static int max_items;

private:
    int secret_ = 0;
    void recount();
};

Money parse(const char* text, std::size_t length);
Money parse(const char* text);
void apply(Cart& cart, Money&& discount) noexcept;
char* const* argv_like(int*& slot);
template <class T>
T convert(const Money& m);

namespace tax {
double rate(Currency c);
inline int rounding = 2;
}  // namespace tax

namespace detail {
int cache_size();
}  // namespace detail
}  // namespace shop

namespace geo_a {
struct Point {
    double x, y;
};
double dist(const Point& a, const Point& b);
inline constexpr int dims = 2;
}  // namespace geo_a

namespace geo_b {  // the same api in another order, with other private and detail parts
inline constexpr int dims = 2;
struct Point {
    double x, y;

private:
    int cache_ = 0;
};
namespace detail {
int helper();
}
double dist(Point const& from, Point const& to) { return from.x - to.x; }
}  // namespace geo_b

namespace geo_c {  // aliases from detail are looked through
namespace detail {
using real = double;
}
struct Point {
    detail::real x, y;
};
namespace detail {
using cref = const Point&;
}
detail::real dist(detail::cref a, detail::cref b);
inline constexpr int dims = 2;
}  // namespace geo_c

namespace geo_d {  // parameters by value
struct Point {
    double x, y;
};
double dist(Point a, Point b);
inline constexpr int dims = 2;
}  // namespace geo_d

namespace geo_e {  // a new default argument
struct Point {
    double x, y;
};
double dist(const Point& a, const Point& b, int norm = 2);
inline constexpr int dims = 2;
}  // namespace geo_e

namespace geo_f {  // a variable changes type
struct Point {
    double x, y;
};
double dist(const Point& a, const Point& b);
inline constexpr long dims = 2;
}  // namespace geo_f

// the api guard: fails to compile when the public api of geo_a changes
static_assert(surface::hash<^^geo_a> == 0x11552bc56ae2a317);

template <std::meta::info A, std::meta::info B>
void compare(const char* what) {
    std::println("{}: {}", what, surface::hash<A> == surface::hash<B> ? "same api" : "api changed");
}

int main() {
    std::print("{}", surface::snapshot<^^shop>);
    std::println("hash of shop: {:#018x}", surface::hash<^^shop>);
    std::print("{}", surface::snapshot<^^geo_a>);
    std::println("hash of geo_a: {:#018x}", surface::hash<^^geo_a>);
    compare<^^geo_a, ^^geo_b>("geo_b reordered");
    compare<^^geo_a, ^^geo_c>("geo_c aliases");
    compare<^^geo_a, ^^geo_d>("geo_d by value");
    compare<^^geo_a, ^^geo_e>("geo_e default argument");
    compare<^^geo_a, ^^geo_f>("geo_f long dims");
}
```

It must print exactly:

```
class Cart
class Money
ctor Cart()
ctor Cart(unsigned long)
enum Cart::State: int
enum Currency: unsigned char
enumerator Cart::State::open = 0
enumerator Cart::State::paid = 10
enumerator Currency::eur = 1
enumerator Currency::gbp = 4
enumerator Currency::usd = 2
field Money::cents: long long
field Money::currency: Currency
fn Money::zero(Currency = ?) -> Money
fn apply(Cart&, Money&&) noexcept -> void
fn argv_like(int*&) -> char* const*
fn parse(char const*) -> Money
fn parse(char const*, unsigned long) -> Money
fn tax::rate(Currency) -> double
method Cart::add(unsigned long, int = ?) -> void
method Cart::label() const -> char const*
method Cart::state() const noexcept -> Cart::State
method Cart::total() const -> Money
method Money::operator==(Money const&) const -> bool
method Money::scaled(double) const noexcept -> Money
template convert
using id_t = unsigned long
var Cart::max_items: int
var tax::rounding: int
var version: int const
hash of shop: 0xe51dc5ffc47ecff0
class Point
field Point::x: double
field Point::y: double
fn dist(Point const&, Point const&) -> double
var dims: int const
hash of geo_a: 0x11552bc56ae2a317
geo_b reordered: same api
geo_c aliases: same api
geo_d by value: api changed
geo_e default argument: api changed
geo_f long dims: api changed
```

Write the complete header to `solution.hpp` in the current directory.
