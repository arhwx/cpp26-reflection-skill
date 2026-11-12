I want something like Zig's `@typeInfo` for debugging: `type_dump<T>()` prints the structure of any type as an indented tree. The type names must be the same on every compiler, so the compiler's own type spelling can't be used.

API:
- `template <class T> std::string type_dump()`: returns the dump of `T`, one line per node, each line ending in `'\n'`.

Type names, written in a postfix style:
- An alias is replaced by the type it stands for, everywhere.
- Fundamental types: `void`, `nullptr_t`, `bool`, `char`, `wchar_t`, `char8_t`, `char16_t`, `char32_t`, `signed char`, `unsigned char`, `short`, `unsigned short`, `int`, `unsigned int`, `long`, `unsigned long`, `long long`, `unsigned long long`, `float`, `double`, `long double`.
- A class, union or enum: its unqualified name. A specialization of a class template: the template's unqualified name, then `<`, the arguments separated by `, `, and `>` (`List<>` when there are none). A type argument is written by these rules, an `int` value in decimal, a `bool` value as `true` or `false`. A class without a name is `(unnamed)`.
- A cv-qualified type: the name without the qualifiers, then ` const`, ` volatile` or ` const volatile`.
- Pointer, lvalue reference, rvalue reference: the name of the type they refer to, then `*`, `&` or `&&`.
- Array: the element type's name, then `[N]`, or `[]` without a bound. For an array of const elements the `const` belongs to the element: `int const[3]`.
- Function type: the return type's name, then the parameter types' names in parentheses, separated by `, `: `double(Shape const&)`. A pointer to it is `double(Shape const&)*`.

Lines:
- A node line is `2 * depth` spaces, then `<role>: ` (the root has no role and no `: `), then `<name> (<kind>)`. The root has depth 0, and children are one level deeper than their parent.
- Kinds: `void`; `null pointer`; `bool`; `character` for `char`, `wchar_t`, `char8_t`, `char16_t` and `char32_t`; `signed integer` for `signed char`, `short`, `int`, `long` and `long long`; `unsigned integer` for their unsigned versions; `floating point`; `pointer`; `lvalue reference`; `rvalue reference`; `array`; `function`; `enum` for an unscoped enum and `enum class` for a scoped one; `union`; and `class` for every other class, whether declared with `struct` or `class`.
- A cv-qualified type that is not an array gets the line of its unqualified type, with `, const`, `, volatile` or `, const volatile` added after the kind, and the same children.

Children, in this order:
- Pointer or reference: one child with role `to`, the type it refers to.
- Array: one child with role `element`.
- Function type: a child with role `returns`, then one child with role `param` per parameter, in order.
- Enum: a child with role `underlying`, its underlying type. Then one line per enumerator in declaration order, `<enumerator> = <value>`, with the value in decimal.
- Class or union: first, if it is a specialization of a class template, one child per template argument in order, with role `arg`. A type argument is a node; a value argument is the line `arg: <value>` with no kind and no children. Then one child per direct base class in declaration order, with role `base`. Then one child per non-static data member in declaration order, private ones included, with role `.` followed by the member's name. Static members and member functions are not shown.
- A class, union or enum is expanded only on its first line in the dump. Every later line for the same type, with any cv-qualification, has `, see above` added at the end inside the parentheses, and no children. Each call of `type_dump` starts fresh.

The test uses no bit-fields, member pointers or unnamed members. Every enum has a fixed underlying type, and template value arguments are `int` or `bool`.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <print>
#include <string>

enum class Color : unsigned char { red = 1, green = 2, blue = 4 };

enum Level : short { low = -1, mid = 0, high = 1 };

struct Point {
    int x;
    int y;
};

template <class T, int N>
struct Buffer {
    T items[N];
    int used;
};

template <class... Ts>
struct List {};

template <bool On>
struct Flag {};

template <int Delta>
struct Offset {};

struct Node {
    long value;
    Node* next;
    const Node* prev;
};

union Number {
    int i;
    double d;
};

class Shape : public Point {
public:
    Color color;
    const char* label;
    double (*area)(const Shape&);
    Buffer<Point, 2> corners;
    struct {
        short lo;
        short hi;
    } range;

private:
    Number weight;
};

using Id = unsigned long long;

struct Entry {
    Id id;
    Level level;
    Number value;
    List<Id, Flag<true>, Offset<-2>> tags;
    Point& anchor;
    float scores[3];
};

template <class T>
void show(const char* title) {
    std::println("== {}", title);
    std::print("{}", type_dump<T>());
}

int main() {
    show<int>("int");
    show<const char* const&>("reference to const pointer");
    show<Shape>("Shape");
    show<Node>("Node");
    show<Entry>("Entry");
    show<int (*[2])(char, Point&&)>("array of function pointers");
    show<void(Color, const Color&)>("function type");
    show<List<>>("empty list");
    show<const volatile Id>("const volatile alias");
}
```

It must print exactly:

```
== int
int (signed integer)
== reference to const pointer
char const* const& (lvalue reference)
  to: char const* const (pointer, const)
    to: char const (character, const)
== Shape
Shape (class)
  base: Point (class)
    .x: int (signed integer)
    .y: int (signed integer)
  .color: Color (enum class)
    underlying: unsigned char (unsigned integer)
    red = 1
    green = 2
    blue = 4
  .label: char const* (pointer)
    to: char const (character, const)
  .area: double(Shape const&)* (pointer)
    to: double(Shape const&) (function)
      returns: double (floating point)
      param: Shape const& (lvalue reference)
        to: Shape const (class, const, see above)
  .corners: Buffer<Point, 2> (class)
    arg: Point (class, see above)
    arg: 2
    .items: Point[2] (array)
      element: Point (class, see above)
    .used: int (signed integer)
  .range: (unnamed) (class)
    .lo: short (signed integer)
    .hi: short (signed integer)
  .weight: Number (union)
    .i: int (signed integer)
    .d: double (floating point)
== Node
Node (class)
  .value: long (signed integer)
  .next: Node* (pointer)
    to: Node (class, see above)
  .prev: Node const* (pointer)
    to: Node const (class, const, see above)
== Entry
Entry (class)
  .id: unsigned long long (unsigned integer)
  .level: Level (enum)
    underlying: short (signed integer)
    low = -1
    mid = 0
    high = 1
  .value: Number (union)
    .i: int (signed integer)
    .d: double (floating point)
  .tags: List<unsigned long long, Flag<true>, Offset<-2>> (class)
    arg: unsigned long long (unsigned integer)
    arg: Flag<true> (class)
      arg: true
    arg: Offset<-2> (class)
      arg: -2
  .anchor: Point& (lvalue reference)
    to: Point (class)
      .x: int (signed integer)
      .y: int (signed integer)
  .scores: float[3] (array)
    element: float (floating point)
== array of function pointers
int(char, Point&&)*[2] (array)
  element: int(char, Point&&)* (pointer)
    to: int(char, Point&&) (function)
      returns: int (signed integer)
      param: char (character)
      param: Point&& (rvalue reference)
        to: Point (class)
          .x: int (signed integer)
          .y: int (signed integer)
== function type
void(Color, Color const&) (function)
  returns: void (void)
  param: Color (enum class)
    underlying: unsigned char (unsigned integer)
    red = 1
    green = 2
    blue = 4
  param: Color const& (lvalue reference)
    to: Color const (enum class, const, see above)
== empty list
List<> (class)
== const volatile alias
unsigned long long const volatile (unsigned integer, const volatile)
```

Write the complete header to `solution.hpp` in the current directory.
