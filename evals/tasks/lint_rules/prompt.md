I want our architecture and style rules checked by the compiler instead of a separate lint tool: naming rules, the rule of five, a padding budget for hot structs, and layering rules between namespaces in the style of ArchUnit (lower layers must not use classes from higher layers). One call scans a whole namespace tree and returns a report.

Everything goes in `namespace lint`:
- `lint::max_padding{n}`: an annotation on a class, written `struct [[=lint::max_padding{0}]] Order { ... };`. `n` is an `int`, the most padding bytes the class may have.
- `consteval std::string_view lint::report(std::meta::info ns, std::vector<std::meta::info> layers)`: checks every scanned class of the namespace `ns` and returns the report described below. `layers` is a list of namespaces, lowest layer first, and may be empty.

Scanned classes:
- Every complete class type declared with `struct` or `class` directly in `ns` or in a namespace nested in `ns` at any depth. Unions and enums are not checked, and neither are classes nested in classes.
- The test has no class templates in the scanned namespaces.
- The qualified name of a class is the names of its enclosing namespaces from the outermost one, then its own name, joined with `::`, with no leading `::`, for example `app::core::Order`.

Checks, done for each class in this order:
1. Class name: if the name is not UpperCamelCase, the problem is `type name is not UpperCamelCase`. UpperCamelCase means the first character is an ASCII uppercase letter and every character is an ASCII letter or digit.
2. Member names: for each non-static data member, whatever its access, in declaration order, if its name is not snake_case, the problem is `member <name> is not snake_case`. snake_case means the first character is an ASCII lowercase letter and every character is an ASCII lowercase letter, digit or `_`.
3. Rule of five: the five special members are, in this order, destructor, copy constructor, copy assignment, move constructor, move assignment. A special member is declared when the class declares it itself, including with `= default` or `= delete`; implicitly declared ones do not count. If the class declares at least one of the five but not all of them, the problem is `rule of five: declares <declared> but not <missing>`, where both lists name the special members in the order above, joined with `, `.
4. Padding: for a class annotated with `lint::max_padding{n}`, its padding is its size minus the sum of the sizes of its non-static data members. If the padding is more than `n`, the problem is `padding <p> bytes, limit <n>`. Annotated classes have no base classes and no bit-fields.
5. Layering: a class is in layer `i` when `layers[i]` is one of its enclosing namespaces, directly or further out. A class that is in no layer is not checked. For a class in layer `i`, its direct base classes are checked first, in declaration order, then its non-static data members in declaration order. For each of them, find the first class it uses that is in a layer `j > i`, as defined below. If there is one, the problem is `base uses <qualified name> from a higher layer` for a base class and `member <name> uses <qualified name> from a higher layer` for a member.

The classes a type uses: resolve type aliases, then remove references, pointers, arrays and `const`/`volatile` at every level, as in `ui::Widget* const* owners[2]`, until none is left. If the result is a specialization of a class template (the test only uses standard ones, such as `std::vector` or `std::map`), the classes it uses are the classes used by each of its type template arguments, in order, recursively. Otherwise, if the result is a class, that class is the one class it uses. Only the type itself counts, never the members of the class it names. "First" means first in this left to right, depth-first order.

The report: one line per problem, `<qualified class name>: <problem>`. The classes are sorted by qualified name in ascending order, comparing the names as `std::string`s do; the problems of one class come in the order of the checks above. Lines are joined with `\n`, with no newline at the end. The report is an empty string when there are no problems.

The annotation spelling must compile exactly as it appears in the test.

Use C++26 static reflection. We build with GCC 16 (`g++ -std=c++26 -freflection`), and it should also build with the clang-p2996 fork (`clang++ -std=c++26 -freflection-latest -stdlib=libc++`).

This is the test program (`test.cpp`):

```cpp
#include "solution.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <print>
#include <string>
#include <vector>

namespace app {

namespace ui {
struct Widget;
}

namespace db {
struct Row {
    std::int64_t id;
    double value;
};

class Connection {
public:
    Connection() = default;
    ~Connection() {}
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;
    Connection(Connection&&) = default;
    Connection& operator=(Connection&&) = default;

private:
    int handle_ = 0;
};

struct row_cache {
    std::vector<Row> rows;
    int hitCount;
};

union Raw {
    int asInt;
    float asFloat;
};
}  // namespace db

namespace core {
struct [[=lint::max_padding{0}]] Order {
    char kind;
    double price;
    std::int32_t qty;
};

struct [[=lint::max_padding{4}]] Quote {
    double price;
    std::int32_t qty;
    char kind;
};

struct Session {
    ~Session() {}
    Session(const Session& other) = default;
    std::string user;
    int userId = 0;
};

class Mover {
public:
    Mover(Mover&&) = default;
    Mover& operator=(Mover&&) = default;
};

struct Cart {
    std::vector<Order> orders;
    db::Row* last;
    std::unique_ptr<ui::Widget> view;
    int count;
};

struct Cache {
    std::map<int, std::vector<const ui::Widget*>> by_id;
    std::map<int, db::Row> rows;
};

namespace detail {
struct Node {
    Node* next;
    ui::Widget* const* owners[2];
};
}  // namespace detail
}  // namespace core

namespace ui {
struct Widget {
    int width;
    core::Order* order;
};

struct Panel : Widget {
    Widget main;
    db::Row rows[4];
};
}  // namespace ui

namespace core {
struct Screen : ui::Panel {
    int Layer;
};
}  // namespace core

namespace tools {
struct Dump {
    ui::Widget widget;
    core::Cart cart;
};
struct HTTPClient2 {};
}  // namespace tools

struct Config {
    std::string db_host;
};

}  // namespace app

namespace clean {
struct [[=lint::max_padding{3}]] Point {
    int x;
    int y;
    char tag;
};
}  // namespace clean

int main() {
    std::println("app:");
    std::println("{}", lint::report(^^app, {^^app::db, ^^app::core, ^^app::ui}));
    std::println("app::ui with ui at the bottom:");
    std::println("{}", lint::report(^^app::ui, {^^app::ui, ^^app::db, ^^app::core}));
    std::println("app::tools as the lowest layer:");
    std::println("{}", lint::report(^^app::tools, {^^app::tools, ^^app::ui}));
    constexpr std::string_view none = lint::report(^^clean, {});
    std::println("clean: {}", none.empty() ? "no problems" : none);
}
```

It must print exactly:

```
app:
app::core::Cache: member by_id uses app::ui::Widget from a higher layer
app::core::Cart: member view uses app::ui::Widget from a higher layer
app::core::Mover: rule of five: declares move constructor, move assignment but not destructor, copy constructor, copy assignment
app::core::Order: padding 11 bytes, limit 0
app::core::Screen: member Layer is not snake_case
app::core::Screen: base uses app::ui::Panel from a higher layer
app::core::Session: member userId is not snake_case
app::core::Session: rule of five: declares destructor, copy constructor but not copy assignment, move constructor, move assignment
app::core::detail::Node: member owners uses app::ui::Widget from a higher layer
app::db::row_cache: type name is not UpperCamelCase
app::db::row_cache: member hitCount is not snake_case
app::ui with ui at the bottom:
app::ui::Panel: member rows uses app::db::Row from a higher layer
app::ui::Widget: member order uses app::core::Order from a higher layer
app::tools as the lowest layer:
app::tools::Dump: member widget uses app::ui::Widget from a higher layer
clean: no problems
```

Write the complete header to `solution.hpp` in the current directory.
